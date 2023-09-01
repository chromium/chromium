// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

constexpr int kMouseRemappableFlags = ui::EF_BACK_MOUSE_BUTTON |
                                      ui::EF_FORWARD_MOUSE_BUTTON |
                                      ui::EF_MIDDLE_MOUSE_BUTTON;

constexpr int kGraphicsTabletRemappableFlags =
    ui::EF_RIGHT_MOUSE_BUTTON | ui::EF_BACK_MOUSE_BUTTON |
    ui::EF_FORWARD_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON;

bool IsMouseButtonEvent(const ui::MouseEvent& mouse_event) {
  return mouse_event.type() == ui::ET_MOUSE_PRESSED ||
         mouse_event.type() == ui::ET_MOUSE_RELEASED;
}

bool IsMouseRemappableButton(int flags) {
  return (flags & kMouseRemappableFlags) != 0;
}

bool IsGraphicsTabletRemappableButton(int flags) {
  return (flags & kGraphicsTabletRemappableFlags) != 0;
}

int GetRemappableMouseEventFlags(
    PeripheralCustomizationEventRewriter::DeviceType device_type) {
  switch (device_type) {
    case PeripheralCustomizationEventRewriter::DeviceType::kMouse:
      return kMouseRemappableFlags;
    case PeripheralCustomizationEventRewriter::DeviceType::kGraphicsTablet:
      return kGraphicsTabletRemappableFlags;
  }
}

mojom::ButtonPtr GetButtonFromMouseEventFlag(int flag) {
  switch (flag) {
    case ui::EF_LEFT_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kLeft);
    case ui::EF_RIGHT_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kRight);
    case ui::EF_MIDDLE_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kMiddle);
    case ui::EF_FORWARD_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kForward);
    case ui::EF_BACK_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kBack);
  }
  NOTREACHED_NORETURN();
}

int ConvertButtonToFlags(const mojom::Button& button) {
  if (button.is_customizable_button()) {
    switch (button.get_customizable_button()) {
      case mojom::CustomizableButton::kLeft:
        return ui::EF_LEFT_MOUSE_BUTTON;
      case mojom::CustomizableButton::kRight:
        return ui::EF_RIGHT_MOUSE_BUTTON;
      case mojom::CustomizableButton::kMiddle:
        return ui::EF_MIDDLE_MOUSE_BUTTON;
      case mojom::CustomizableButton::kForward:
        return ui::EF_FORWARD_MOUSE_BUTTON;
      case mojom::CustomizableButton::kBack:
        return ui::EF_BACK_MOUSE_BUTTON;
      case mojom::CustomizableButton::kExtra:
        return ui::EF_FORWARD_MOUSE_BUTTON;
      case mojom::CustomizableButton::kSide:
        return ui::EF_BACK_MOUSE_BUTTON;
    }
  }

  if (button.is_vkey()) {
    switch (button.get_vkey()) {
      case ui::VKEY_LWIN:
      case ui::VKEY_RWIN:
        return ui::EF_COMMAND_DOWN;
      case ui::VKEY_CONTROL:
        return ui::EF_CONTROL_DOWN;
      case ui::VKEY_SHIFT:
      case ui::VKEY_LSHIFT:
      case ui::VKEY_RSHIFT:
        return ui::EF_SHIFT_DOWN;
      case ui::VKEY_MENU:
        return ui::EF_ALT_DOWN;
      default:
        return ui::EF_NONE;
    }
  }

  return ui::EF_NONE;
}

}  // namespace

PeripheralCustomizationEventRewriter::PeripheralCustomizationEventRewriter() =
    default;
PeripheralCustomizationEventRewriter::~PeripheralCustomizationEventRewriter() =
    default;

absl::optional<PeripheralCustomizationEventRewriter::DeviceType>
PeripheralCustomizationEventRewriter::GetDeviceTypeToObserve(int device_id) {
  if (mice_to_observe_.contains(device_id)) {
    return DeviceType::kMouse;
  }
  if (graphics_tablets_to_observe_.contains(device_id)) {
    return DeviceType::kGraphicsTablet;
  }
  return absl::nullopt;
}

void PeripheralCustomizationEventRewriter::StartObservingMouse(int device_id) {
  mice_to_observe_.insert(device_id);
}

void PeripheralCustomizationEventRewriter::StartObservingGraphicsTablet(
    int device_id) {
  graphics_tablets_to_observe_.insert(device_id);
}

void PeripheralCustomizationEventRewriter::StopObserving() {
  graphics_tablets_to_observe_.clear();
  mice_to_observe_.clear();
}

bool PeripheralCustomizationEventRewriter::NotifyMouseEventObserving(
    const ui::MouseEvent& mouse_event,
    DeviceType device_type) {
  if (!IsMouseButtonEvent(mouse_event)) {
    return false;
  }

  // Make sure the button is remappable for the current `device_type`.
  switch (device_type) {
    case DeviceType::kMouse:
      if (!IsMouseRemappableButton(mouse_event.changed_button_flags())) {
        return false;
      }
      break;
    case DeviceType::kGraphicsTablet:
      if (!IsGraphicsTabletRemappableButton(
              mouse_event.changed_button_flags())) {
        return false;
      }
      break;
  }

  if (mouse_event.type() != ui::ET_MOUSE_PRESSED) {
    return true;
  }

  const auto button =
      GetButtonFromMouseEventFlag(mouse_event.changed_button_flags());
  for (auto& observer : observers_) {
    switch (device_type) {
      case DeviceType::kMouse:
        observer.OnMouseButtonPressed(mouse_event.source_device_id(), *button);
        break;
      case DeviceType::kGraphicsTablet:
        observer.OnGraphicsTabletButtonPressed(mouse_event.source_device_id(),
                                               *button);
        break;
    }
  }

  return true;
}

bool PeripheralCustomizationEventRewriter::NotifyKeyEventObserving(
    const ui::KeyEvent& key_event,
    DeviceType device_type) {
  // Observers should only be notified on key presses.
  if (key_event.type() != ui::ET_KEY_PRESSED) {
    return true;
  }

  const auto button = mojom::Button::NewVkey(key_event.key_code());
  for (auto& observer : observers_) {
    switch (device_type) {
      case DeviceType::kMouse:
        observer.OnMouseButtonPressed(key_event.source_device_id(), *button);
        break;
      case DeviceType::kGraphicsTablet:
        observer.OnGraphicsTabletButtonPressed(key_event.source_device_id(),
                                               *button);
        break;
    }
  }

  return true;
}

bool PeripheralCustomizationEventRewriter::RewriteEventFromButton(
    const ui::Event& event,
    const mojom::Button& button,
    std::unique_ptr<ui::Event>& rewritten_event) {
  auto* remapping_action = GetRemappingAction(event.source_device_id(), button);
  if (!remapping_action) {
    return false;
  }

  if (remapping_action->is_action()) {
    if (event.type() == ui::ET_KEY_PRESSED ||
        event.type() == ui::ET_MOUSE_PRESSED) {
      // Every accelerator supported by peripheral customization is not impacted
      // by the accelerator passed. Therefore, passing an empty accelerator will
      // cause no issues.
      Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
          remapping_action->get_action(), /*accelerator=*/{});
    }

    return true;
  }

  if (remapping_action->is_key_event()) {
    const auto& key_event = remapping_action->get_key_event();
    const ui::EventType event_type = (event.type() == ui::ET_MOUSE_PRESSED ||
                                      event.type() == ui::ET_KEY_PRESSED)
                                         ? ui::ET_KEY_PRESSED
                                         : ui::ET_KEY_RELEASED;
    rewritten_event = std::make_unique<ui::KeyEvent>(
        event_type, key_event->vkey,
        static_cast<ui::DomCode>(key_event->dom_code),
        key_event->modifiers | event.flags(),
        static_cast<ui::DomKey>(key_event->dom_key), event.time_stamp());
    rewritten_event->set_source_device_id(event.source_device_id());
  }

  return false;
}

ui::EventDispatchDetails PeripheralCustomizationEventRewriter::RewriteKeyEvent(
    const ui::KeyEvent& key_event,
    const Continuation continuation) {
  auto device_type_to_observe =
      GetDeviceTypeToObserve(key_event.source_device_id());
  if (device_type_to_observe) {
    if (NotifyKeyEventObserving(key_event, *device_type_to_observe)) {
      return DiscardEvent(continuation);
    }
  }

  std::unique_ptr<ui::Event> rewritten_event;
  if (RewriteEventFromButton(key_event,
                             *mojom::Button::NewVkey(key_event.key_code()),
                             rewritten_event)) {
    return DiscardEvent(continuation);
  }

  if (!rewritten_event) {
    rewritten_event = std::make_unique<ui::KeyEvent>(key_event);
  }

  RemoveRemappedModifiers(*rewritten_event);
  return SendEvent(continuation, rewritten_event.get());
}

ui::EventDispatchDetails
PeripheralCustomizationEventRewriter::RewriteMouseEvent(
    const ui::MouseEvent& mouse_event,
    const Continuation continuation) {
  auto device_type_to_observe =
      GetDeviceTypeToObserve(mouse_event.source_device_id());
  if (device_type_to_observe) {
    if (NotifyMouseEventObserving(mouse_event, *device_type_to_observe)) {
      return DiscardEvent(continuation);
    }

    // Otherwise, the flags must be cleared for the remappable buttons so they
    // do not affect the application while the mouse is meant to be observed.
    ui::MouseEvent rewritten_event = mouse_event;
    const int remappable_flags =
        GetRemappableMouseEventFlags(*device_type_to_observe);
    rewritten_event.set_flags(rewritten_event.flags() & ~remappable_flags);
    rewritten_event.set_changed_button_flags(
        rewritten_event.changed_button_flags() & ~remappable_flags);
    return SendEvent(continuation, &rewritten_event);
  }

  std::unique_ptr<ui::Event> rewritten_event;
  if (IsMouseButtonEvent(mouse_event)) {
    if (mouse_event.changed_button_flags() &&
        RewriteEventFromButton(
            mouse_event,
            *GetButtonFromMouseEventFlag(mouse_event.changed_button_flags()),
            rewritten_event)) {
      return DiscardEvent(continuation);
    }
  }

  if (!rewritten_event) {
    if (mouse_event.IsMouseWheelEvent()) {
      rewritten_event = std::make_unique<ui::MouseWheelEvent>(
          *mouse_event.AsMouseWheelEvent());
    } else {
      rewritten_event = std::make_unique<ui::MouseEvent>(mouse_event);
    }
  }

  RemoveRemappedModifiers(*rewritten_event);
  return SendEvent(continuation, rewritten_event.get());
}

ui::EventDispatchDetails PeripheralCustomizationEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DCHECK(features::IsPeripheralCustomizationEnabled());

  if (event.IsMouseEvent()) {
    return RewriteMouseEvent(*event.AsMouseEvent(), continuation);
  }

  if (event.IsKeyEvent()) {
    return RewriteKeyEvent(*event.AsKeyEvent(), continuation);
  }

  return SendEvent(continuation, &event);
}

void PeripheralCustomizationEventRewriter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PeripheralCustomizationEventRewriter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const mojom::RemappingAction*
PeripheralCustomizationEventRewriter::GetRemappingAction(
    int device_id,
    const mojom::Button& button) {
  const auto button_remapping_list_iter =
      button_remappings_for_testing_.find(device_id);
  if (button_remapping_list_iter == button_remappings_for_testing_.end()) {
    return nullptr;
  }

  const auto& button_remapping_list = button_remapping_list_iter->second;
  const auto action_iter = base::ranges::find(
      button_remapping_list, button,
      [](const std::pair<mojom::ButtonPtr, mojom::RemappingActionPtr>& entry) {
        return *entry.first;
      });
  if (action_iter != button_remapping_list.end()) {
    return action_iter->second.get();
  }

  return nullptr;
}

void PeripheralCustomizationEventRewriter::RemoveRemappedModifiers(
    ui::Event& event) {
  const auto button_remapping_list_iter =
      button_remappings_for_testing_.find(event.source_device_id());
  if (button_remapping_list_iter == button_remappings_for_testing_.end()) {
    return;
  }

  int modifiers = 0;
  for (const auto& [button, action] : button_remapping_list_iter->second) {
    modifiers |= ConvertButtonToFlags(*button);
  }

  // TODO(dpad): This logic isn't quite correct. If a second devices is holding
  // "Ctrl" and the original device has a button that is "Ctrl" that is
  // remapped, this will behave incorrectly as it will remove "Ctrl". Instead,
  // this needs to track what keys are being pressed by the device that have
  // modifiers attached to them. For now, this is close enough to being correct.
  event.set_flags(event.flags() & ~modifiers);
}

void PeripheralCustomizationEventRewriter::SetRemappingActionForTesting(
    int device_id,
    mojom::ButtonPtr button,
    mojom::RemappingActionPtr remapping_action) {
  button_remappings_for_testing_[device_id].emplace_back(
      std::move(button), std::move(remapping_action));
}

}  // namespace ash
