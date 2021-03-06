#include "ui.h"
#include "checkbox.h"
#include "group.h"
#include "subgroup.h"
#include "slider.h"
#include "scroll_bar.h"
#include "select.h"
#include "button.h"
#include "colour.h"
#include "element.h"
#include "../mousecursor.h"

#include <algorithm>

UI::UI() : selectedElement(0) {
    font = fontmanager.grab("FreeSans.ttf", 12);
    font.dropShadow(true);
    double_click_interval = 0.5f;
    double_click_timer = double_click_interval;

    left_pressed = false;
    left_drag    = false;
    scrolling    = false;
    cursor_pos   = vec2(0.0f);

    background_colour = vec4(0.3f, 0.3f, 0.3f, 0.67f);
    solid_colour      = vec4(0.7f, 0.7f, 0.7f, 1.0f);
    tint_colour       = vec4(0.0f, 0.8f, 0.0f, 1.0f);
    text_colour       = vec4(1.0f);
    ui_alpha          = vec4(1.0f);

    shader            = shadermanager.grab("ui/ui");
}

UI::~UI() {
    clear();
}

void UI::clear() {
    deselect();
    for(UIElement* e: elements) {
        delete e;
    }
    elements.clear();
}

void UI::addElement(UIElement* e) {
    e->setUI(this);
    elements.push_back(e);
}

void UI::removeElement(UIElement* e) {
    if(e == getSelected()) selectElement(0);

    elements.erase(std::remove(elements.begin(), elements.end(), e), elements.end());
    e->setUI(0);
}

UIElement* UI::getSelected() {
    return selectedElement;
}

vec4 UI::getBackgroundColour() {
    return background_colour * ui_alpha;
}

vec4 UI::getSolidColour() {
    return solid_colour * ui_alpha;
}

vec4 UI::getTextColour() {
    return text_colour * ui_alpha;
}

vec4 UI::getTintColour() {
    return tint_colour * ui_alpha;
}

vec4 UI::getAlpha() {
    return ui_alpha;
}

bool UI::elementsByType(std::list<UIElement*>& found, int type) {

    bool success = false;

    for(UIElement* e: elements) {
        if(e->elementsByType(found, type)) success = true;
    }

    return success;
}

void UI::elementsAt(const vec2& pos, std::list<UIElement*>& found_elements) {

    for(UIElement* e: elements) {
        if(e->isVisible()) e->elementsAt(pos, found_elements);
    }

    // sort by zindex before returning
    found_elements.sort(UIElement::zindex_sort);
}

UIElement* UI::scrollableElementAt(const vec2& pos) {

    std::list<UIElement*> found_elements;

    elementsAt(pos, found_elements);

    for(UIElement* found : found_elements) {

        //debugLog("scrollable %d", found->zindex);

        if(found) {
            //debugLog("element %s (%d) zindex %d", found->getElementName().c_str(), found->getType(), found->zindex);
        }

        while(found && !found->isScrollable()) {
            found = found->parent;
        }

        if(found) {
            //debugLog("scrolling element %s zindex %d", found->getElementName().c_str(), found->zindex);
            return found;
        }
    }

    return 0;
}

void UI::deselect() {
    if(!selectedElement) return;

    selectedElement->setSelected(false);
    selectedElement = 0;
}

void UI::selectElement(UIElement* element) {
    if(selectedElement != 0) {
        selectedElement->setSelected(false);
    }
    selectedElement = element;

    if(element!=0) {
        element->setSelected(true);
    }
}

UIElement* UI::selectElementAt(const vec2& pos) {

    double_click_timer = 0.0f;

    std::list<UIElement*> found_elements;

    elementsAt(pos, found_elements);

    UIElement* found = 0;

    for(UIElement* e : found_elements) {
        //debugLog("select %s %d", e->getElementName().c_str(), e->zindex);
        if(!e->isSelectable()) continue;
        found = e;
        break;
    }

    if(selectedElement == found) return selectedElement;

    if(selectedElement != 0) {
        selectedElement->setSelected(false);
    }

    //if(found) debugLog("selected element %s (%d) zindex %d", found->getElementName().c_str(), found->getType(), found->zindex);

    if(!found) {
        selectedElement = 0;
        return 0;
    }

    selectedElement = found;

    selectedElement->setSelected(true);

    return selectedElement;
}

void UI::update(float dt) {

    //update/pick elements by zindex
    std::sort(elements.begin(), elements.end(), UIElement::zindex_sort);

    if(ui_alpha.w < 1.0f) {
        ui_alpha.w = glm::min(ui_alpha.w + dt * 0.5f, 1.0f);
    }

    if(double_click_timer<double_click_interval) double_click_timer += dt;

    for(UIElement* e: elements) {
        e->update(dt);
    }

    for(UIElement* e: elements) {
        e->updatePos(vec2(0.0f, 0.0f));
    }
}

void UI::setTextured(bool textured) {
    shader->setBool("use_texture", textured);
    shader->applyUniforms();
}

void UI::setIntensity(float intensity) {
    shader->setFloat("intensity", intensity);
    shader->applyUniforms();
}

void UI::drawText(float x, float y, const char *str, ...) {

    char text[4096];

    va_list vl;

    va_start (vl, str);
    vsnprintf (text, 4096, str, vl);
    va_end (vl);

    drawText(x, y, std::string(text));
}

void UI::drawText(float x, float y, const std::string& text) {

    shader->setBool("text", true);
    shader->applyUniforms();

    font.draw(x, y, text);

    shader->setBool("text", false);
    shader->applyUniforms();
}

void UI::draw() {
    glActiveTexture(GL_TEXTURE0);

    shader->setSampler2D("texture", 0);
    shader->use();

    setIntensity(1.0f);
    setTextured(true);

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    std::vector<UIElement*> draw_elements(elements);

    //draw elements by reverse zindex
    std::sort(draw_elements.begin(), draw_elements.end(), UIElement::reverse_zindex_sort);

    for(UIElement* e: draw_elements) {
        if(e->isVisible()) e->draw();
    }

    shader->unbind();

    glDisable(GL_DEPTH_TEST);
}

UIElement* UI::processMouse(MouseCursor& cursor) {

    bool mousemove = (cursor.getPos() != cursor_pos);

    cursor_pos = cursor.getPos();

    if(cursor.leftClick()) {

        left_pressed = true;
        return click(cursor);
        
    } else if(cursor.leftButtonPressed()) {

        if(mousemove) {
            left_drag = true;
            return drag(cursor);
        }
        
    } else {
        if(left_pressed) {
            if(left_drag) {
                left_drag = false;
            }
            left_pressed = false;
        }
    }

    int scroll_amount = cursor.scrollWheel();

    if(scroll_amount != 0) {
        scrolling = true;
        return scroll(cursor);
    } else if(scrolling) {
        scrolling = false;
    }

    // call idle if nothing happened
    // not sure about this 'idle' concept really
    // might be better to indicate the type of change in the action class

    UIElement* selected = getSelected();
    if(selected!=0) selected->idle();


    return 0;
}

void UI::drawOutline() {
    for(UIElement* e: elements) {
        if(e->isVisible()) e->drawOutline();
    }
}

char UI::toChar(SDL_KeyboardEvent *e) {

    int unicode = e->keysym.unicode;

    if( unicode > 0x80 && unicode <= 0 ) return 0;

    char c         = unicode;
    bool uppercase = SDL_GetModState() & KMOD_SHIFT;

    if(uppercase) {
        return toupper(c);
    }

    return c;
}

bool UI::keyPress(SDL_KeyboardEvent *e) {

    UIElement* selected = getSelected();

    if(!selected) return false;

    if(e->keysym.unicode == SDLK_ESCAPE) {
        deselect();
        return selected->isEditable();
    }

    char c = toChar(e);

    return selected->keyPress(e, c);
}

UIElement* UI::scroll(MouseCursor& cursor) {
    int scroll_amount = cursor.scrollWheel();

    if(!scroll_amount) return 0;

    bool dir = (scroll_amount > 0);

    UIElement* el = scrollableElementAt(cursor.getPos());

    if(!el) return 0;

    if(el->isSelectable()) selectElement(el);

    el->scroll(dir);

    return el;
}

UIElement* UI::drag(const MouseCursor& cursor) {

    UIElement* selected = getSelected();

    if(!selected) return 0;

    selected->drag(cursor.getPos());

    return selected;
}

UIElement* UI::click(const MouseCursor& cursor) {

    UIElement* previous = getSelected();
    bool double_click   = double_click_timer < 0.5f;

    vec2 pos = cursor.getPos();

    UIElement* selected = selectElementAt(pos);

    if(!selected) return 0;

    if(previous == selected && double_click) {
        selected->doubleClick(pos);
    } else {
        selected->click(pos);
    }

    return selected;
}

UIColour* UI::getActiveColour() {

    std::list<UIElement*> found;
    elementsByType(found, UI_COLOUR);

    for(UIElement* e: found) {
        UIColour* c = (UIColour*)e;

        if(c->active) return c;
    }

    return 0;
}
