// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"

#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/events/event_constants.h"

namespace arc {
namespace input_overlay {

int ModifierDomCodeToEventFlag(ui::DomCode code) {
  switch (code) {
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return ui::EF_ALT_DOWN;
    case ui::DomCode::CAPS_LOCK:
      return ui::EF_CAPS_LOCK_ON;
    case ui::DomCode::META_LEFT:
    case ui::DomCode::META_RIGHT:
      return ui::EF_COMMAND_DOWN;
    case ui::DomCode::SHIFT_LEFT:
    case ui::DomCode::SHIFT_RIGHT:
      return ui::EF_SHIFT_DOWN;
    default:
      return ui::EF_NONE;
  }
}

bool IsSameDomCode(ui::DomCode a, ui::DomCode b) {
  return a == b ||
         (ModifierDomCodeToEventFlag(a) != ui::EF_NONE &&
          ModifierDomCodeToEventFlag(a) == ModifierDomCodeToEventFlag(b));
}

InputElement::InputElement() {}
InputElement::~InputElement() = default;

// static
std::unique_ptr<InputElement> InputElement::CreateActionTapKeyElement(
    ui::DomCode key) {
  auto element = std::make_unique<InputElement>();
  element->input_sources_ = InputSource::IS_KEYBOARD;
  element->keys_.emplace_back(key);
  if (ModifierDomCodeToEventFlag(key) != ui::EF_NONE)
    element->is_modifier_key_ = true;
  return element;
}

// static
std::unique_ptr<InputElement> InputElement::CreateActionTapMouseElement(
    const std::string& mouse_action) {
  auto element = std::make_unique<InputElement>();
  element->input_sources_ = InputSource::IS_MOUSE;
  element->mouse_lock_required_ = true;
  element->mouse_action_ = mouse_action;
  element->mouse_types_.emplace(ui::ET_MOUSE_PRESSED);
  element->mouse_types_.emplace(ui::ET_MOUSE_RELEASED);
  if (mouse_action == kPrimaryClick) {
    element->mouse_flags_ = ui::EF_LEFT_MOUSE_BUTTON;
  } else {
    DCHECK(mouse_action == kSecondaryClick);
    element->mouse_flags_ = ui::EF_RIGHT_MOUSE_BUTTON;
  }
  return element;
}

// static
std::unique_ptr<InputElement> InputElement::CreateActionMoveKeyElement(
    const std::vector<ui::DomCode>& keys) {
  auto element = std::make_unique<InputElement>();
  element->input_sources_ = InputSource::IS_KEYBOARD;
  std::copy(keys.begin(), keys.end(), std::back_inserter(element->keys_));
  // There are four and only four keys representing move up, left, down and
  // right.
  DCHECK(element->keys_.size() == kActionMoveKeysSize);
  return element;
}

// static
std::unique_ptr<InputElement> InputElement::CreateActionMoveMouseElement(
    const std::string& mouse_action) {
  auto element = std::make_unique<InputElement>();
  element->input_sources_ = InputSource::IS_MOUSE;
  element->mouse_lock_required_ = true;
  element->mouse_action_ = mouse_action;
  if (mouse_action == kHoverMove) {
    element->mouse_types_.emplace(ui::ET_MOUSE_ENTERED);
    element->mouse_types_.emplace(ui::ET_MOUSE_MOVED);
    element->mouse_types_.emplace(ui::ET_MOUSE_EXITED);
  } else {
    DCHECK(mouse_action == kPrimaryDragMove ||
           mouse_action == kSecondaryDragMove);
    element->mouse_types_.emplace(ui::ET_MOUSE_PRESSED);
    element->mouse_types_.emplace(ui::ET_MOUSE_DRAGGED);
    element->mouse_types_.emplace(ui::ET_MOUSE_RELEASED);
    if (mouse_action == kPrimaryDragMove) {
      element->mouse_flags_ = ui::EF_LEFT_MOUSE_BUTTON;
    } else {
      element->mouse_flags_ = ui::EF_RIGHT_MOUSE_BUTTON;
    }
  }
  return element;
}

bool InputElement::operator==(const InputElement& other) const {
  if (this->input_sources_ != other.input_sources())
    return false;
  bool equal = true;
  if (!!(this->input_sources_ & InputSource::IS_KEYBOARD))
    equal = equal && (this->keys_ == other.keys());
  if (!!(this->input_sources_ & InputSource::IS_MOUSE))
    equal = equal && (this->mouse_action_ == other.mouse_action());
  return equal;
}

}  // namespace input_overlay
}  // namespace arc
