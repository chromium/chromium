// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include <linux/input.h>

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/mouse_button_property.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

namespace {

constexpr int kMouseRemappableFlags = ui::EF_BACK_MOUSE_BUTTON |
                                      ui::EF_FORWARD_MOUSE_BUTTON |
                                      ui::EF_MIDDLE_MOUSE_BUTTON;

constexpr int kGraphicsTabletRemappableFlags =
    ui::EF_RIGHT_MOUSE_BUTTON | ui::EF_BACK_MOUSE_BUTTON |
    ui::EF_FORWARD_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON;

constexpr auto kStaticActionToMouseButtonFlag =
    base::MakeFixedFlatMap<mojom::StaticShortcutAction, ui::EventFlags>({
        {mojom::StaticShortcutAction::kLeftClick, ui::EF_LEFT_MOUSE_BUTTON},
        {mojom::StaticShortcutAction::kRightClick, ui::EF_RIGHT_MOUSE_BUTTON},
        {mojom::StaticShortcutAction::kMiddleClick, ui::EF_MIDDLE_MOUSE_BUTTON},
    });

mojom::KeyEvent GetStaticShortcutAction(mojom::StaticShortcutAction action) {
  mojom::KeyEvent key_event;
  switch (action) {
    case mojom::StaticShortcutAction::kDisable:
    case mojom::StaticShortcutAction::kLeftClick:
    case mojom::StaticShortcutAction::kRightClick:
    case mojom::StaticShortcutAction::kMiddleClick:
      NOTREACHED_NORETURN();
    case mojom::StaticShortcutAction::kCopy:
      key_event = mojom::KeyEvent(
          ui::VKEY_C, static_cast<int>(ui::DomCode::US_C),
          static_cast<int>(ui::DomKey::Constant<'c'>::Character),
          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kPaste:
      key_event = mojom::KeyEvent(
          ui::VKEY_V, static_cast<int>(ui::DomCode::US_V),
          static_cast<int>(ui::DomKey::Constant<'v'>::Character),
          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kUndo:
      key_event = mojom::KeyEvent(
          ui::VKEY_Z, static_cast<int>(ui::DomCode::US_Z),
          static_cast<int>(ui::DomKey::Constant<'z'>::Character),
          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kRedo:
      key_event = mojom::KeyEvent(
          ui::VKEY_Z, static_cast<int>(ui::DomCode::US_Z),
          static_cast<int>(ui::DomKey::Constant<'z'>::Character),
          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kZoomIn:
      key_event = mojom::KeyEvent(
          ui::VKEY_OEM_PLUS, static_cast<int>(ui::DomCode::EQUAL),
          static_cast<int>(ui::DomKey::Constant<'='>::Character),
          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kZoomOut:
      key_event = mojom::KeyEvent(
          ui::VKEY_OEM_MINUS, static_cast<int>(ui::DomCode::MINUS),
          static_cast<int>(ui::DomKey::Constant<'-'>::Character),
          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kPreviousPage:
      key_event = mojom::KeyEvent(ui::VKEY_BROWSER_BACK,
                                  static_cast<int>(ui::DomCode::BROWSER_BACK),
                                  static_cast<int>(ui::DomKey::BROWSER_BACK),
                                  ui::EF_NONE, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kNextPage:
      key_event =
          mojom::KeyEvent(ui::VKEY_BROWSER_FORWARD,
                          static_cast<int>(ui::DomCode::BROWSER_FORWARD),
                          static_cast<int>(ui::DomKey::BROWSER_FORWARD),
                          ui::EF_NONE, /*key_display=*/"");
      break;
  }
  return key_event;
}

int ConvertKeyCodeToFlags(ui::KeyboardCode key_code) {
  switch (key_code) {
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
    case ui::VKEY_RMENU:
      return ui::EF_ALT_DOWN;
    default:
      return ui::EF_NONE;
  }
}

std::unique_ptr<ui::Event> RewriteEventToKeyEvent(
    const ui::Event& event,
    const mojom::KeyEvent& key_event) {
  const ui::EventType event_type = (event.type() == ui::ET_MOUSE_PRESSED ||
                                    event.type() == ui::ET_KEY_PRESSED)
                                       ? ui::ET_KEY_PRESSED
                                       : ui::ET_KEY_RELEASED;
  int flags_to_apply = key_event.modifiers;
  // Do not apply modifier flags when the key is a modifier and it is a release
  // event. Modifier keys do not apply their flag on release.
  if (event_type == ui::ET_KEY_RELEASED &&
      key_event.modifiers ==
          static_cast<uint32_t>(ConvertKeyCodeToFlags(key_event.vkey))) {
    flags_to_apply = ui::EF_NONE;
  }
  auto rewritten_event = std::make_unique<ui::KeyEvent>(
      event_type, key_event.vkey, static_cast<ui::DomCode>(key_event.dom_code),
      flags_to_apply | event.flags(),
      static_cast<ui::DomKey>(key_event.dom_key), event.time_stamp());
  rewritten_event->set_source_device_id(event.source_device_id());
  return rewritten_event;
}

std::unique_ptr<ui::Event> RewriteEventToMouseButtonEvent(
    const ui::Event& event,
    mojom::StaticShortcutAction action) {
  auto* flag_iter = kStaticActionToMouseButtonFlag.find(action);
  CHECK(flag_iter != kStaticActionToMouseButtonFlag.end());
  const int characteristic_flag = flag_iter->second;

  auto* screen = display::Screen::GetScreen();
  CHECK(screen);
  auto display = screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
  const gfx::PointF location =
      gfx::ScalePoint(gfx::PointF(screen->GetCursorScreenPoint() -
                                  display.bounds().origin().OffsetFromOrigin()),
                      display.device_scale_factor());

  const ui::EventType type = (event.type() == ui::ET_MOUSE_PRESSED ||
                              event.type() == ui::ET_KEY_PRESSED)
                                 ? ui::ET_MOUSE_PRESSED
                                 : ui::ET_MOUSE_RELEASED;
  auto rewritten_event = std::make_unique<ui::MouseEvent>(
      type, location, location, event.time_stamp(),
      event.flags() | characteristic_flag, characteristic_flag);
  rewritten_event->set_source_device_id(event.source_device_id());
  return rewritten_event;
}

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

mojom::ButtonPtr GetButtonFromMouseEvent(const ui::MouseEvent& mouse_event) {
  switch (mouse_event.changed_button_flags()) {
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
    case ui::EF_BACK_MOUSE_BUTTON:
      break;
  }

  CHECK(mouse_event.changed_button_flags() == ui::EF_FORWARD_MOUSE_BUTTON ||
        mouse_event.changed_button_flags() == ui::EF_BACK_MOUSE_BUTTON);
  auto linux_key_code = ui::GetForwardBackMouseButtonProperty(mouse_event);
  if (!linux_key_code) {
    return (mouse_event.changed_button_flags() == ui::EF_FORWARD_MOUSE_BUTTON)
               ? mojom::Button::NewCustomizableButton(
                     mojom::CustomizableButton::kForward)
               : mojom::Button::NewCustomizableButton(
                     mojom::CustomizableButton::kBack);
  }

  switch (*linux_key_code) {
    case BTN_FORWARD:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kForward);
    case BTN_BACK:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kBack);
    case BTN_SIDE:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kSide);
    case BTN_EXTRA:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kExtra);
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
      case mojom::CustomizableButton::kExtra:
        return ui::EF_FORWARD_MOUSE_BUTTON;
      case mojom::CustomizableButton::kBack:
      case mojom::CustomizableButton::kSide:
        return ui::EF_BACK_MOUSE_BUTTON;
    }
  }

  if (button.is_vkey()) {
    return ConvertKeyCodeToFlags(button.get_vkey());
  }

  return ui::EF_NONE;
}

const mojom::RemappingAction* GetRemappingActionFromMouseSettings(
    const mojom::Button& button,
    const mojom::MouseSettings& settings) {
  const auto button_remapping_iter = base::ranges::find(
      settings.button_remappings, button,
      [](const mojom::ButtonRemappingPtr& entry) { return *entry->button; });
  if (button_remapping_iter != settings.button_remappings.end()) {
    return (*button_remapping_iter)->remapping_action.get();
  }

  return nullptr;
}

const mojom::RemappingAction* GetRemappingActionFromGraphicsTabletSettings(
    const mojom::Button& button,
    const mojom::GraphicsTabletSettings& settings) {
  const auto pen_button_remapping_iter = base::ranges::find(
      settings.pen_button_remappings, button,
      [](const mojom::ButtonRemappingPtr& entry) { return *entry->button; });
  if (pen_button_remapping_iter != settings.pen_button_remappings.end()) {
    return (*pen_button_remapping_iter)->remapping_action.get();
  }

  const auto tablet_button_remapping_iter = base::ranges::find(
      settings.tablet_button_remappings, button,
      [](const mojom::ButtonRemappingPtr& entry) { return *entry->button; });
  if (tablet_button_remapping_iter != settings.tablet_button_remappings.end()) {
    return (*tablet_button_remapping_iter)->remapping_action.get();
  }

  return nullptr;
}

int GetRemappedModifiersFromMouseSettings(
    const mojom::MouseSettings& settings) {
  int modifiers = 0;
  for (const auto& button_remapping : settings.button_remappings) {
    if (button_remapping->remapping_action) {
      modifiers |= ConvertButtonToFlags(*button_remapping->button);
    }
  }
  return modifiers;
}

int GetRemappedModifiersFromGraphicsTabletSettings(
    const mojom::GraphicsTabletSettings& settings) {
  int modifiers = 0;
  for (const auto& button_remapping : settings.pen_button_remappings) {
    modifiers |= ConvertButtonToFlags(*button_remapping->button);
  }
  for (const auto& button_remapping : settings.tablet_button_remappings) {
    modifiers |= ConvertButtonToFlags(*button_remapping->button);
  }
  return modifiers;
}

}  // namespace

// Compares the `DeviceIdButton` struct based on first the device id, and then
// the button stored within the `DeviceIdButton` struct.
bool operator<(
    const PeripheralCustomizationEventRewriter::DeviceIdButton& left,
    const PeripheralCustomizationEventRewriter::DeviceIdButton& right) {
  if (right.device_id != left.device_id) {
    return left.device_id < right.device_id;
  }

  // If both are VKeys, compare them.
  if (left.button->is_vkey() && right.button->is_vkey()) {
    return left.button->get_vkey() < right.button->get_vkey();
  }

  // If both are customizable buttons, compare them.
  if (left.button->is_customizable_button() &&
      right.button->is_customizable_button()) {
    return left.button->get_customizable_button() <
           right.button->get_customizable_button();
  }

  // Otherwise, return true if the lhs is a VKey as they mismatch and VKeys
  // should be considered less than customizable buttons.
  return left.button->is_vkey();
}

PeripheralCustomizationEventRewriter::DeviceIdButton::DeviceIdButton(
    int device_id,
    mojom::ButtonPtr button)
    : device_id(device_id), button(std::move(button)) {}

PeripheralCustomizationEventRewriter::DeviceIdButton::DeviceIdButton(
    DeviceIdButton&& device_id_button)
    : device_id(device_id_button.device_id),
      button(std::move(device_id_button.button)) {}

PeripheralCustomizationEventRewriter::DeviceIdButton::~DeviceIdButton() =
    default;

PeripheralCustomizationEventRewriter::DeviceIdButton&
PeripheralCustomizationEventRewriter::DeviceIdButton::operator=(
    PeripheralCustomizationEventRewriter::DeviceIdButton&& device_id_button) =
    default;

PeripheralCustomizationEventRewriter::PeripheralCustomizationEventRewriter(
    InputDeviceSettingsController* input_device_settings_controller)
    : input_device_settings_controller_(input_device_settings_controller) {}
PeripheralCustomizationEventRewriter::~PeripheralCustomizationEventRewriter() =
    default;

std::optional<PeripheralCustomizationEventRewriter::DeviceType>
PeripheralCustomizationEventRewriter::GetDeviceTypeToObserve(int device_id) {
  if (mice_to_observe_.contains(device_id)) {
    return DeviceType::kMouse;
  }
  if (graphics_tablets_to_observe_.contains(device_id)) {
    return DeviceType::kGraphicsTablet;
  }
  return std::nullopt;
}

void PeripheralCustomizationEventRewriter::StartObservingMouse(
    int device_id,
    bool can_rewrite_key_event) {
  if (can_rewrite_key_event) {
    mice_to_observe_key_events_.insert(device_id);
  }
  mice_to_observe_.insert(device_id);
}

void PeripheralCustomizationEventRewriter::StartObservingGraphicsTablet(
    int device_id) {
  graphics_tablets_to_observe_.insert(device_id);
}

void PeripheralCustomizationEventRewriter::StopObserving() {
  graphics_tablets_to_observe_.clear();
  mice_to_observe_.clear();
  mice_to_observe_key_events_.clear();
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

  const auto button = GetButtonFromMouseEvent(mouse_event);
  switch (device_type) {
    case DeviceType::kMouse:
      input_device_settings_controller_->OnMouseButtonPressed(
          mouse_event.source_device_id(), *button);
      break;
    case DeviceType::kGraphicsTablet:
      input_device_settings_controller_->OnGraphicsTabletButtonPressed(
          mouse_event.source_device_id(), *button);
      break;
  }

  return true;
}

bool PeripheralCustomizationEventRewriter::NotifyKeyEventObserving(
    const ui::KeyEvent& key_event,
    DeviceType device_type) {
  // Only mice that are in the mice_to_observe_key_events_ set should be allowed
  // to observe key events.
  if (device_type == DeviceType::kMouse &&
      !mice_to_observe_key_events_.contains(key_event.source_device_id())) {
    return false;
  }

  // Observers should only be notified on key presses.
  if (key_event.type() != ui::ET_KEY_PRESSED) {
    return true;
  }

  const auto button = mojom::Button::NewVkey(key_event.key_code());
  switch (device_type) {
    case DeviceType::kMouse:
      input_device_settings_controller_->OnMouseButtonPressed(
          key_event.source_device_id(), *button);
      break;
    case DeviceType::kGraphicsTablet:
      input_device_settings_controller_->OnGraphicsTabletButtonPressed(
          key_event.source_device_id(), *button);
      break;
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

  if (remapping_action->is_accelerator_action()) {
    if (event.type() == ui::ET_KEY_PRESSED ||
        event.type() == ui::ET_MOUSE_PRESSED) {
      // Every accelerator supported by peripheral customization is not impacted
      // by the accelerator passed. Therefore, passing an empty accelerator will
      // cause no issues.
      Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
          remapping_action->get_accelerator_action(), /*accelerator=*/{});
    }

    return true;
  }

  if (remapping_action->is_key_event()) {
    const auto& key_event = remapping_action->get_key_event();
    rewritten_event = RewriteEventToKeyEvent(event, *key_event);
  }

  if (remapping_action->is_static_shortcut_action()) {
    const auto static_action = remapping_action->get_static_shortcut_action();
    if (static_action == mojom::StaticShortcutAction::kDisable) {
      // Return true to discard the event.
      return true;
    }

    if (kStaticActionToMouseButtonFlag.contains(static_action)) {
      rewritten_event = RewriteEventToMouseButtonEvent(event, static_action);
    } else {
      rewritten_event = RewriteEventToKeyEvent(
          event, GetStaticShortcutAction(
                     remapping_action->get_static_shortcut_action()));
    }
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
  mojom::ButtonPtr button = mojom::Button::NewVkey(key_event.key_code());
  bool updated_button_map = false;
  if (RewriteEventFromButton(key_event, *button, rewritten_event)) {
    return DiscardEvent(continuation);
  }

  // Update pressed button map now if either there was no rewrite or if its not
  // a mouse release event.
  if (!rewritten_event || rewritten_event->type() != ui::ET_MOUSE_RELEASED) {
    updated_button_map = true;
    UpdatePressedButtonMap(std::move(button), key_event, rewritten_event);
  }

  if (!rewritten_event) {
    rewritten_event = std::make_unique<ui::KeyEvent>(key_event);
  }

  RemoveRemappedModifiers(*rewritten_event);
  ApplyRemappedModifiers(*rewritten_event);

  if (!updated_button_map) {
    UpdatePressedButtonMap(std::move(button), key_event, rewritten_event);
  }

  return SendEvent(continuation, rewritten_event.get());
}

void PeripheralCustomizationEventRewriter::UpdatePressedButtonMap(
    mojom::ButtonPtr button,
    const ui::Event& original_event,
    const std::unique_ptr<ui::Event>& rewritten_event) {
  DeviceIdButton device_id_button_key =
      DeviceIdButton{original_event.source_device_id(), std::move(button)};

  // If the button is released, the entry must be removed from the map.
  if (original_event.type() == ui::ET_MOUSE_RELEASED ||
      original_event.type() == ui::ET_KEY_RELEASED) {
    device_button_to_flags_.erase(std::move(device_id_button_key));
    return;
  }

  const int key_event_flags =
      (rewritten_event && rewritten_event->IsKeyEvent())
          ? ConvertKeyCodeToFlags(rewritten_event->AsKeyEvent()->key_code())
          : 0;
  const int mouse_event_flags =
      (rewritten_event && rewritten_event->IsMouseEvent())
          ? rewritten_event->AsMouseEvent()->changed_button_flags()
          : 0;

  const int combined_flags = key_event_flags | mouse_event_flags;
  if (!combined_flags) {
    return;
  }

  // Add the entry to the map with the flags that must be applied to other
  // events.
  device_button_to_flags_.insert_or_assign(std::move(device_id_button_key),
                                           combined_flags);
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
    std::unique_ptr<ui::Event> rewritten_event = CloneEvent(mouse_event);
    const int remappable_flags =
        GetRemappableMouseEventFlags(*device_type_to_observe);
    rewritten_event->set_flags(rewritten_event->flags() & ~remappable_flags);
    if (rewritten_event->IsMouseEvent()) {
      auto& rewritten_mouse_event = *rewritten_event->AsMouseEvent();
      rewritten_mouse_event.set_changed_button_flags(
          rewritten_mouse_event.changed_button_flags() & ~remappable_flags);
    }
    return SendEvent(continuation, rewritten_event.get());
  }

  std::unique_ptr<ui::Event> rewritten_event;
  mojom::ButtonPtr button;
  bool updated_button_map = false;
  if (IsMouseButtonEvent(mouse_event) && mouse_event.changed_button_flags()) {
    button = GetButtonFromMouseEvent(mouse_event);
    if (RewriteEventFromButton(mouse_event, *button, rewritten_event)) {
      return DiscardEvent(continuation);
    }
    // Update pressed button map now if either there was no rewrite or if its
    // not a mouse release event.
    if (!rewritten_event || rewritten_event->type() != ui::ET_MOUSE_RELEASED) {
      updated_button_map = true;
      UpdatePressedButtonMap(std::move(button), mouse_event, rewritten_event);
    }
  }

  if (!rewritten_event) {
    rewritten_event = CloneEvent(mouse_event);
  }

  RemoveRemappedModifiers(*rewritten_event);
  ApplyRemappedModifiers(*rewritten_event);

  if (!updated_button_map) {
    UpdatePressedButtonMap(std::move(button), mouse_event, rewritten_event);
  }

  return SendEvent(continuation, rewritten_event.get());
}

ui::EventDispatchDetails PeripheralCustomizationEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DCHECK(features::IsPeripheralCustomizationEnabled() ||
         ::features::IsShortcutCustomizationEnabled());

  if (event.IsMouseEvent()) {
    return RewriteMouseEvent(*event.AsMouseEvent(), continuation);
  }

  if (event.IsKeyEvent()) {
    return RewriteKeyEvent(*event.AsKeyEvent(), continuation);
  }

  return SendEvent(continuation, &event);
}

const mojom::RemappingAction*
PeripheralCustomizationEventRewriter::GetRemappingAction(
    int device_id,
    const mojom::Button& button) {
  const auto* mouse_settings =
      input_device_settings_controller_->GetMouseSettings(device_id);
  if (mouse_settings) {
    return GetRemappingActionFromMouseSettings(button, *mouse_settings);
  }

  const auto* graphics_tablet_settings =
      input_device_settings_controller_->GetGraphicsTabletSettings(device_id);
  if (graphics_tablet_settings) {
    return GetRemappingActionFromGraphicsTabletSettings(
        button, *graphics_tablet_settings);
  }

  return nullptr;
}

void PeripheralCustomizationEventRewriter::RemoveRemappedModifiers(
    ui::Event& event) {
  int modifier_flags = 0;
  if (const auto* mouse_settings =
          input_device_settings_controller_->GetMouseSettings(
              event.source_device_id());
      mouse_settings) {
    modifier_flags = GetRemappedModifiersFromMouseSettings(*mouse_settings);
  } else if (const auto* graphics_tablet_settings =
                 input_device_settings_controller_->GetGraphicsTabletSettings(
                     event.source_device_id());
             graphics_tablet_settings) {
    modifier_flags = GetRemappedModifiersFromGraphicsTabletSettings(
        *graphics_tablet_settings);
  }

  // TODO(dpad): This logic isn't quite correct. If a second devices is holding
  // "Ctrl" and the original device has a button that is "Ctrl" that is
  // remapped, this will behave incorrectly as it will remove "Ctrl". Instead,
  // this needs to track what keys are being pressed by the device that have
  // modifiers attached to them. For now, this is close enough to being correct.
  event.set_flags(event.flags() & ~modifier_flags);
}

void PeripheralCustomizationEventRewriter::ApplyRemappedModifiers(
    ui::Event& event) {
  int flags = 0;
  for (const auto& [_, flag] : device_button_to_flags_) {
    flags |= flag;
  }
  event.set_flags(event.flags() | flags);
}

std::unique_ptr<ui::Event> PeripheralCustomizationEventRewriter::CloneEvent(
    const ui::Event& event) {
  std::unique_ptr<ui::Event> cloned_event = event.Clone();
  // SetNativeEvent must be called explicitly as native events are not copied
  // on ChromeOS by default. This is because `PlatformEvent` is a pointer by
  // default, so its lifetime can not be guaranteed in general. In this case,
  // the lifetime of  `rewritten_event` is guaranteed to be less than the
  // original `mouse_event`.
  SetNativeEvent(*cloned_event, event.native_event());
  return cloned_event;
}

}  // namespace ash
