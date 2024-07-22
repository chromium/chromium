// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"

#include <iterator>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace arc::input_overlay {

InputElement::InputElement() {}

InputElement::InputElement(ui::DomCode code) {
  input_sources_ = InputSource::IS_KEYBOARD;
  keys_.emplace_back(code);
  if (ModifierDomCodeToEventFlag(code) != ui::EF_NONE) {
    is_modifier_key_ = true;
  }
}

InputElement::InputElement(const InputElement& other) = default;
InputElement::~InputElement() = default;

// static
std::unique_ptr<InputElement> InputElement::CreateActionTapKeyElement(
    ui::DomCode key) {
  auto element = std::make_unique<InputElement>(key);
  return element;
}

// static
std::unique_ptr<InputElement> InputElement::CreateActionTapMouseElement(
    const std::string& mouse_action) {
  auto element = std::make_unique<InputElement>();
  element->input_sources_ = InputSource::IS_MOUSE;
  element->mouse_lock_required_ = true;
  element->mouse_action_ = ConvertToMouseActionEnum(mouse_action);
  element->mouse_types_.emplace(ui::EventType::kMousePressed);
  element->mouse_types_.emplace(ui::EventType::kMouseReleased);
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
  base::ranges::copy(keys, std::back_inserter(element->keys_));
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
  element->mouse_action_ = ConvertToMouseActionEnum(mouse_action);
  if (mouse_action == kHoverMove) {
    element->mouse_types_.emplace(ui::EventType::kMouseEntered);
    element->mouse_types_.emplace(ui::EventType::kMouseMoved);
    element->mouse_types_.emplace(ui::EventType::kMouseExited);
  } else {
    DCHECK(mouse_action == kPrimaryDragMove ||
           mouse_action == kSecondaryDragMove);
    element->mouse_types_.emplace(ui::EventType::kMousePressed);
    element->mouse_types_.emplace(ui::EventType::kMouseDragged);
    element->mouse_types_.emplace(ui::EventType::kMouseReleased);
    if (mouse_action == kPrimaryDragMove) {
      element->mouse_flags_ = ui::EF_LEFT_MOUSE_BUTTON;
    } else {
      element->mouse_flags_ = ui::EF_RIGHT_MOUSE_BUTTON;
    }
  }
  return element;
}

// static
std::unique_ptr<InputElement> InputElement::ConvertFromProto(
    const InputElementProto& proto) {
  auto input_element = std::make_unique<InputElement>();
  if ((proto.input_sources() & InputSource::IS_KEYBOARD) ==
      InputSource::IS_KEYBOARD) {
    input_element->set_input_sources(InputSource::IS_KEYBOARD);
    std::vector<ui::DomCode> codes;
    for (const auto& code_str : proto.dom_codes()) {
      auto code = ui::KeycodeConverter::CodeStringToDomCode(code_str);
      codes.emplace_back(code);
    }
    input_element->SetKeys(codes);
  }

  if ((proto.input_sources() & InputSource::IS_MOUSE) ==
      InputSource::IS_MOUSE) {
    // TODO(cuicuiruan): Implement for post MVP when mouse overlay is enabled.
    NOTIMPLEMENTED();
  }

  return input_element;
}

bool InputElement::IsOverlapped(const InputElement& input_element) const {
  if (input_sources_ != input_element.input_sources() ||
      input_sources_ == InputSource::IS_NONE) {
    return false;
  }
  if (input_sources_ == InputSource::IS_KEYBOARD) {
    for (auto key : input_element.keys()) {
      if (key != ui::DomCode::NONE && base::Contains(keys_, key)) {
        return true;
      }
    }
    return false;
  }
  return mouse_action_ == input_element.mouse_action();
}

bool InputElement::IsUnbound() const {
  if (input_sources_ == InputSource::IS_NONE) {
    return true;
  }

  if (IsInputSourceSet(InputSource::IS_KEYBOARD)) {
    for (const auto key : keys()) {
      if (key != ui::DomCode::NONE) {
        // Consider it as bound if there is at lease one key is bound.
        return false;
      }
    }
  }

  if (IsInputSourceSet(InputSource::IS_MOUSE)) {
    return mouse_action_ == MouseAction::NONE;
  }

  return true;
}

void InputElement::SetKey(size_t index, ui::DomCode code) {
  DCHECK(index < keys_.size());
  if (index >= keys_.size()) {
    return;
  }
  keys_[index] = code;
}

void InputElement::SetKeys(std::vector<ui::DomCode>& keys) {
  keys_.clear();
  base::ranges::copy(keys, std::back_inserter(keys_));
}

int InputElement::GetIndexOfKey(ui::DomCode key) const {
  auto it = base::ranges::find(keys_, key);
  return it == keys_.end() ? -1 : it - keys_.begin();
}

std::unique_ptr<InputElementProto> InputElement::ConvertToProto() {
  auto proto = std::make_unique<InputElementProto>();
  proto->set_input_sources(input_sources_);
  proto->set_mouse_action(mouse_action_);
  for (const auto key : keys_)
    proto->add_dom_codes(ui::KeycodeConverter::DomCodeToCodeString(key));
  return proto;
}

bool InputElement::operator==(const InputElement& other) const {
  if (this->input_sources_ != other.input_sources()) {
    return false;
  }
  bool equal = true;
  if (!!(this->input_sources_ & InputSource::IS_KEYBOARD)) {
    equal = equal && (this->keys_ == other.keys());
  }
  if (!!(this->input_sources_ & InputSource::IS_MOUSE)) {
    equal = equal && (this->mouse_action_ == other.mouse_action());
  }
  return equal;
}

bool InputElement::operator!=(const InputElement& other) const {
  return !(*this == other);
}

bool InputElement::IsInputSourceSet(InputSource input_source) const {
  return (input_sources_ & input_source) == input_source;
}

}  // namespace arc::input_overlay
