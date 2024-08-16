// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>

#include <iterator>
#include <memory>
#include <optional>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_settings_logging.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/env.h"
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/mouse_button_property.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

namespace {

using RemappingActionResult =
    PeripheralCustomizationEventRewriter::RemappingActionResult;

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
      NOTREACHED();
    case mojom::StaticShortcutAction::kCopy:
      key_event =
          mojom::KeyEvent(ui::VKEY_C, static_cast<int>(ui::DomCode::US_C),
                          static_cast<int>(ui::DomKey::FromCharacter('c')),
                          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kPaste:
      key_event =
          mojom::KeyEvent(ui::VKEY_V, static_cast<int>(ui::DomCode::US_V),
                          static_cast<int>(ui::DomKey::FromCharacter('v')),
                          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kUndo:
      key_event =
          mojom::KeyEvent(ui::VKEY_Z, static_cast<int>(ui::DomCode::US_Z),
                          static_cast<int>(ui::DomKey::FromCharacter('z')),
                          ui::EF_CONTROL_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kRedo:
      key_event = mojom::KeyEvent(
          ui::VKEY_Z, static_cast<int>(ui::DomCode::US_Z),
          static_cast<int>(ui::DomKey::FromCharacter('z')),
          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kZoomIn:
      key_event = mojom::KeyEvent(
          ui::VKEY_OEM_PLUS, static_cast<int>(ui::DomCode::EQUAL),
          static_cast<int>(ui::DomKey::FromCharacter('=')), ui::EF_CONTROL_DOWN,
          /*key_display=*/"");
      break;
    case mojom::StaticShortcutAction::kZoomOut:
      key_event = mojom::KeyEvent(
          ui::VKEY_OEM_MINUS, static_cast<int>(ui::DomCode::MINUS),
          static_cast<int>(ui::DomKey::FromCharacter('-')), ui::EF_CONTROL_DOWN,
          /*key_display=*/"");
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
    case ui::VKEY_RCONTROL:
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

int ConvertModifierKeyToFlags(ui::mojom::ModifierKey modifier_key) {
  switch (modifier_key) {
    case ui::mojom::ModifierKey::kMeta:
      return ui::EF_COMMAND_DOWN;
    case ui::mojom::ModifierKey::kControl:
      return ui::EF_CONTROL_DOWN;
    case ui::mojom::ModifierKey::kAlt:
      return ui::EF_ALT_DOWN;
    case ui::mojom::ModifierKey::kFunction:
      return ui::EF_FUNCTION_DOWN;
    case ui::mojom::ModifierKey::kEscape:
    case ui::mojom::ModifierKey::kBackspace:
    case ui::mojom::ModifierKey::kAssistant:
    case ui::mojom::ModifierKey::kCapsLock:
    case ui::mojom::ModifierKey::kVoid:
    case ui::mojom::ModifierKey::kIsoLevel5ShiftMod3:
    case ui::mojom::ModifierKey::kRightAlt:
      return ui::EF_NONE;
  }
}

bool AreScrollWheelEventRewritesAllowed(
    mojom::CustomizationRestriction customization_restriction) {
  switch (customization_restriction) {
    case mojom::CustomizationRestriction::kDisallowCustomizations:
    case mojom::CustomizationRestriction::kDisableKeyEventRewrites:
    case mojom::CustomizationRestriction::kAllowAlphabetKeyEventRewrites:
    case mojom::CustomizationRestriction::
        kAllowAlphabetOrNumberKeyEventRewrites:
    case mojom::CustomizationRestriction::kAllowTabEventRewrites:
    case mojom::CustomizationRestriction::kAllowFKeyRewrites:
      return false;
    case mojom::CustomizationRestriction::kAllowHorizontalScrollWheelRewrites:
    case mojom::CustomizationRestriction::kAllowCustomizations:
      return true;
  }
}

template <typename Iterator>
std::vector<std::unique_ptr<ui::Event>> RewriteModifiers(
    const ui::Event& event,
    uint32_t modifiers_to_press,
    uint32_t modifiers_already_pressed,
    bool pressed,
    Iterator begin,
    Iterator end) {
  std::vector<std::unique_ptr<ui::Event>> rewritten_events;

  // Keeps track of the modifier flags that must be applied to the next
  // rewritten event. For key presses, this should start at
  // `0` and go to `modifiers_to_press`. For releases, it should do the opposite
  // as we start will all modifiers down.
  uint32_t modifiers_pressed = pressed ? 0u : modifiers_to_press;

  for (auto iter = begin; iter != end; iter++) {
    if (!(iter->flag & modifiers_to_press)) {
      continue;
    }

    if (pressed) {
      modifiers_pressed += iter->flag;
    } else {
      modifiers_pressed -= iter->flag;
    }

    const ui::EventType pressed_or_released_type =
        pressed ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased;
    auto rewritten_modifier_event = std::make_unique<ui::KeyEvent>(
        pressed_or_released_type, iter->key_code, iter->dom_code,
        modifiers_pressed | modifiers_already_pressed |
            ui::EF_IS_CUSTOMIZED_FROM_BUTTON,
        event.time_stamp());
    rewritten_modifier_event->set_source_device_id(event.source_device_id());
    rewritten_events.push_back(std::move(rewritten_modifier_event));
  }

  // Verify our modifier rewriting worked as expected.
  if (pressed) {
    CHECK_EQ(modifiers_to_press, modifiers_pressed);
  } else {
    CHECK_EQ(0u, modifiers_pressed);
  }

  return rewritten_events;
}

std::vector<std::unique_ptr<ui::Event>> GenerateFullKeyEventSequence(
    const ui::Event& event,
    uint32_t modifiers_to_press,
    uint32_t modifiers_already_pressed,
    bool pressed,
    std::unique_ptr<ui::Event> rewritten_event) {
  static constexpr struct {
    ui::KeyboardCode key_code;
    ui::DomCode dom_code;
    ui::EventFlags flag;
  } kModifiers[] = {
      {ui::VKEY_LWIN, ui::DomCode::META_LEFT, ui::EF_COMMAND_DOWN},
      {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN},
      {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN},
      {ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT, ui::EF_SHIFT_DOWN},
  };
  static constexpr auto kModifierSpan = base::make_span(kModifiers);

  CHECK(rewritten_event);
  std::vector<std::unique_ptr<ui::Event>> rewritten_events;

  if (pressed) {
    // If it is a key press, we rewrite the modifiers in forward order and then
    // add the main rewritten event after the press events.
    rewritten_events =
        RewriteModifiers(event, modifiers_to_press, modifiers_already_pressed,
                         pressed, kModifierSpan.begin(), kModifierSpan.end());
    rewritten_events.push_back(std::move(rewritten_event));
  } else {
    // For key releases, we add the main rewritten event first and then append
    // the rewritten modifiers after. The modifiers must be rewritten in reverse
    // order from the `kModifiers` array.
    rewritten_events.push_back(std::move(rewritten_event));
    auto modifier_events =
        RewriteModifiers(event, modifiers_to_press, modifiers_already_pressed,
                         pressed, kModifierSpan.rbegin(), kModifierSpan.rend());
    rewritten_events.insert(rewritten_events.end(),
                            std::make_move_iterator(modifier_events.begin()),
                            std::make_move_iterator(modifier_events.end()));
  }

  return rewritten_events;
}

std::vector<std::unique_ptr<ui::Event>> RewriteEventToKeyEvents(
    const ui::Event& event,
    const mojom::KeyEvent& key_event,
    int flags_to_release,
    bool key_press) {
  const ui::EventType event_type =
      key_press ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased;
  const uint32_t modifier_key_flag = ConvertKeyCodeToFlags(key_event.vkey);

  // `other_modifiers_to_apply` symbolizes the flags that are not handled by
  // `modifier_key_flag` or flags that are already included in the event. The
  // flags remaining in `other_modifiers_to_apply` are the modifiers we must
  // write events for in addition to the main key event.

  // On release events, we should release the flags passed in instead of our
  // computed set of flags in case some modifiers were released since we
  // originally pressed them down.

  // Mouse wheel is an exception here since presses and releases happen
  // atomically. Therefore always release every key you originally pressed.
  const uint32_t other_modifiers_to_apply =
      (key_press || event.type() == ui::EventType::kMousewheel)
          ? (key_event.modifiers & ~event.flags() & ~modifier_key_flag)
          : (flags_to_release & ~event.flags() & ~modifier_key_flag);

  uint32_t applied_modifier_key_flag = modifier_key_flag;
  // Do not apply modifier flags when the key is a modifier and it is a release
  // event. Modifier keys do not apply their flag on release.
  if (event_type == ui::EventType::kKeyReleased &&
      key_event.modifiers == modifier_key_flag) {
    applied_modifier_key_flag = ui::EF_NONE;
  }

  const bool is_rewrite_to_right_alt = key_event.vkey == ui::VKEY_RIGHT_ALT;
  ui::KeyboardCode key_code = key_event.vkey;
  if (is_rewrite_to_right_alt) {
    key_code = ui::VKEY_ASSISTANT;
  }
  // Use ui::DomKey::NONE so the DomKey is recomputed with applicable flags.
  auto rewritten_event = std::make_unique<ui::KeyEvent>(
      event_type, key_code, static_cast<ui::DomCode>(key_event.dom_code),
      applied_modifier_key_flag | other_modifiers_to_apply | event.flags() |
          ui::EF_IS_CUSTOMIZED_FROM_BUTTON,
      ui::DomKey::NONE, event.time_stamp());
  rewritten_event->set_source_device_id(event.source_device_id());
  if (is_rewrite_to_right_alt) {
    ui::SetRightAltProperty(rewritten_event.get());
  }

  return GenerateFullKeyEventSequence(
      event, other_modifiers_to_apply, event.flags(),
      /*pressed=*/event_type == ui::EventType::kKeyPressed,
      std::move(rewritten_event));
}

std::vector<std::unique_ptr<ui::Event>> RewriteEventToKeyEvents(
    const ui::Event& event,
    const mojom::KeyEvent& key_event,
    int flags_to_release) {
  // If the original event is a mouse scroll event, we must generate both a
  // press and release from the single event.
  const bool should_press_and_release =
      event.type() == ui::EventType::kMousewheel;

  const bool key_press = should_press_and_release ||
                         event.type() == ui::EventType::kMousePressed ||
                         event.type() == ui::EventType::kKeyPressed;
  std::vector<std::unique_ptr<ui::Event>> rewritten_events =
      RewriteEventToKeyEvents(event, key_event, flags_to_release, key_press);

  if (should_press_and_release) {
    std::vector<std::unique_ptr<ui::Event>> release_rewritten_events =
        RewriteEventToKeyEvents(event, key_event, flags_to_release, false);
    rewritten_events.reserve(rewritten_events.size() +
                             release_rewritten_events.size());
    rewritten_events.insert(
        rewritten_events.end(),
        std::make_move_iterator(release_rewritten_events.begin()),
        std::make_move_iterator(release_rewritten_events.end()));
  }
  return rewritten_events;
}

// TODO(b/339754921): Add integration test for when the display is rotated and
// adjusted via overscan boundaries.
gfx::PointF GetCurrentCursorLocation() {
  auto* screen = display::Screen::GetScreen();
  CHECK(screen);
  const display::Display display =
      screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());

  // Returns the physical point on the display not considering display
  // orientation.
  gfx::PointF physical_screen_location =
      gfx::PointF(screen->GetCursorScreenPoint() -
                  display.bounds().origin().OffsetFromOrigin());

  // Transpose/flip the point based on the orientation of device.
  auto& display_size = display.size();
  switch (display.rotation()) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      physical_screen_location.Transpose();
      physical_screen_location.set_x(display_size.height() -
                                     physical_screen_location.x());
      break;
    case display::Display::ROTATE_180: {
      physical_screen_location.set_x(display_size.width() -
                                     physical_screen_location.x());
      physical_screen_location.set_y(display_size.height() -
                                     physical_screen_location.y());
      break;
    }
    case display::Display::ROTATE_270:
      physical_screen_location.Transpose();
      physical_screen_location.set_y(display_size.width() -
                                     physical_screen_location.y());
      break;
  }
  // Scale the location to match the users chosen scaling factor then apply
  // overscan insets. Overscan insets are stored as post-scaled values so they
  // must be applied after scaling the original location.
  auto scaled_location =
      gfx::ScalePoint(physical_screen_location, display.device_scale_factor());
  auto overscan_insets =
      Shell::Get()->display_manager()->GetOverscanInsets(display.id());
  scaled_location.set_x(scaled_location.x() + overscan_insets.left());
  scaled_location.set_y(scaled_location.y() + overscan_insets.top());
  return scaled_location;
}

std::vector<std::unique_ptr<ui::Event>> RewriteEventToMouseButtonEvents(
    const ui::Event& event,
    mojom::StaticShortcutAction action) {
  // If the original event is a mouse scroll event, we must generate both a
  // press and release from the single event.
  const bool should_press_and_release =
      event.type() == ui::EventType::kMousewheel;

  std::vector<std::unique_ptr<ui::Event>> rewritten_events;

  auto flag_iter = kStaticActionToMouseButtonFlag.find(action);
  CHECK(flag_iter != kStaticActionToMouseButtonFlag.end());
  const int characteristic_flag = flag_iter->second;

  const gfx::PointF location = GetCurrentCursorLocation();
  const ui::EventType type = (should_press_and_release ||
                              event.type() == ui::EventType::kMousePressed ||
                              event.type() == ui::EventType::kKeyPressed)
                                 ? ui::EventType::kMousePressed
                                 : ui::EventType::kMouseReleased;
  rewritten_events.push_back(std::make_unique<ui::MouseEvent>(
      type, location, location, event.time_stamp(),
      event.flags() | characteristic_flag, characteristic_flag));
  rewritten_events.back()->set_source_device_id(event.source_device_id());

  if (should_press_and_release) {
    rewritten_events.push_back(std::make_unique<ui::MouseEvent>(
        ui::EventType::kMouseReleased, location, location, event.time_stamp(),
        event.flags() | characteristic_flag, characteristic_flag));
    rewritten_events.back()->set_source_device_id(event.source_device_id());
  }

  return rewritten_events;
}

bool IsMouseButtonEvent(const ui::MouseEvent& mouse_event) {
  return mouse_event.type() == ui::EventType::kMousePressed ||
         mouse_event.type() == ui::EventType::kMouseReleased;
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

  NOTREACHED();
}

// Returns the customizable button for the scroll wheel event. Will return null
// if the scroll event is not a horizontal scroll event.
mojom::ButtonPtr GetButtonFromMouseWheelEvent(
    const ui::MouseWheelEvent& mouse_wheel_event) {
  if (mouse_wheel_event.x_offset() == 0) {
    return nullptr;
  }

  if (mouse_wheel_event.x_offset() > 0) {
    return mojom::Button::NewCustomizableButton(
        mojom::CustomizableButton::kScrollRight);
  }

  return mojom::Button::NewCustomizableButton(
      mojom::CustomizableButton::kScrollLeft);
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
      case mojom::CustomizableButton::kScrollLeft:
      case mojom::CustomizableButton::kScrollRight:
        return ui::EF_NONE;
    }
  }

  if (button.is_vkey()) {
    return ConvertKeyCodeToFlags(button.get_vkey());
  }

  return ui::EF_NONE;
}

std::optional<ui::mojom::ModifierKey> ConvertDomCodeToModifierKey(
    ui::DomCode code) {
  switch (code) {
    case ui::DomCode::META_LEFT:
    case ui::DomCode::META_RIGHT:
      return ui::mojom::ModifierKey::kMeta;
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      return ui::mojom::ModifierKey::kControl;
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return ui::mojom::ModifierKey::kAlt;
    case ui::DomCode::CAPS_LOCK:
      return ui::mojom::ModifierKey::kCapsLock;
    case ui::DomCode::BACKSPACE:
      return ui::mojom::ModifierKey::kBackspace;
    case ui::DomCode::LAUNCH_ASSISTANT:
      return ui::mojom::ModifierKey::kAssistant;
    case ui::DomCode::ESCAPE:
      return ui::mojom::ModifierKey::kEscape;
    default:
      return std::nullopt;
  }
}

std::optional<PeripheralCustomizationEventRewriter::RemappingActionResult>
GetRemappingActionFromMouseSettings(const mojom::Button& button,
                                    const mojom::MouseSettings& settings) {
  const auto button_remapping_iter = base::ranges::find(
      settings.button_remappings, button,
      [](const mojom::ButtonRemappingPtr& entry) { return *entry->button; });
  if (button_remapping_iter != settings.button_remappings.end()) {
    const mojom::ButtonRemapping& button_remapping = *(*button_remapping_iter);
    if (!button_remapping.remapping_action) {
      return std::nullopt;
    }

    auto result = PeripheralCustomizationEventRewriter::RemappingActionResult(
        *button_remapping.remapping_action,
        InputDeviceSettingsMetricsManager::PeripheralCustomizationMetricsType::
            kMouse);
    return result;
  }

  return std::nullopt;
}

std::optional<PeripheralCustomizationEventRewriter::RemappingActionResult>
GetRemappingActionFromGraphicsTabletSettings(
    const mojom::Button& button,
    const mojom::GraphicsTabletSettings& settings) {
  const auto pen_button_remapping_iter = base::ranges::find(
      settings.pen_button_remappings, button,
      [](const mojom::ButtonRemappingPtr& entry) { return *entry->button; });
  if (pen_button_remapping_iter != settings.pen_button_remappings.end()) {
    const mojom::ButtonRemapping& button_remapping =
        *(*pen_button_remapping_iter);
    if (!button_remapping.remapping_action) {
      return std::nullopt;
    }

    auto pen_action =
        PeripheralCustomizationEventRewriter::RemappingActionResult(
            *button_remapping.remapping_action,
            InputDeviceSettingsMetricsManager::
                PeripheralCustomizationMetricsType::kGraphicsTabletPen);
    return std::move(pen_action);
  }

  const auto tablet_button_remapping_iter = base::ranges::find(
      settings.tablet_button_remappings, button,
      [](const mojom::ButtonRemappingPtr& entry) { return *entry->button; });
  if (tablet_button_remapping_iter != settings.tablet_button_remappings.end()) {
    const mojom::ButtonRemapping& button_remapping =
        *(*tablet_button_remapping_iter);
    if (!button_remapping.remapping_action) {
      return std::nullopt;
    }

    auto tablet_action =
        PeripheralCustomizationEventRewriter::RemappingActionResult(
            *button_remapping.remapping_action,
            InputDeviceSettingsMetricsManager::
                PeripheralCustomizationMetricsType::kGraphicsTablet);
    return std::move(tablet_action);
  }

  return std::nullopt;
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

// Verify if the keyboard code is an alpha key or punctuation.
bool IsAlphaKeyEvent(const ui::KeyEvent& key_event) {
  return GetKeyInputTypeFromKeyEvent(key_event) ==
         AcceleratorKeyInputType::kAlpha;
}

// Verify if the keyboard code is a number.
bool IsNumberKeyEvent(const ui::KeyEvent& key_event) {
  return GetKeyInputTypeFromKeyEvent(key_event) ==
         AcceleratorKeyInputType::kDigit;
}

void RecordMouseInvalidKeyPressed(InputDeviceSettingsController* controller,
                                  const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::EventType::kKeyReleased ||
      key_event.is_repeat()) {
    return;
  }

  auto* mouse = controller->GetMouse(key_event.source_device_id());
  auto* keyboard = controller->GetKeyboard(key_event.source_device_id());
  if (!mouse) {
    return;
  }

  if (mouse && keyboard) {
    base::UmaHistogramSparse("ChromeOS.Inputs.Mouse.InvalidRegistration.Combo",
                             key_event.key_code());
    return;
  }

  if (mouse) {
    base::UmaHistogramSparse(
        "ChromeOS.Inputs.Mouse.InvalidRegistration.NonCombo",
        key_event.key_code());
  }

  LOG(WARNING) << base::StringPrintf(
      "Mouse '%s' with identifier '%s' attempted to register keyboard code "
      "'%04x'.",
      mouse->name.c_str(), mouse->device_key.c_str(), key_event.key_code());
  base::UmaHistogramSparse("ChromeOS.Inputs.Mouse.InvalidRegistration",
                           key_event.key_code());
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

PeripheralCustomizationEventRewriter::RemappingActionResult::
    RemappingActionResult(
        mojom::RemappingAction& remapping_action,
        InputDeviceSettingsMetricsManager::PeripheralCustomizationMetricsType
            peripheral_kind)
    : remapping_action(remapping_action), peripheral_kind(peripheral_kind) {}

PeripheralCustomizationEventRewriter::RemappingActionResult::
    RemappingActionResult(RemappingActionResult&& result)
    : remapping_action(std::move(result.remapping_action)),
      peripheral_kind(result.peripheral_kind) {}

PeripheralCustomizationEventRewriter::RemappingActionResult::
    ~RemappingActionResult() = default;

PeripheralCustomizationEventRewriter::PeripheralCustomizationEventRewriter(
    InputDeviceSettingsController* input_device_settings_controller)
    : input_device_settings_controller_(input_device_settings_controller) {
  metrics_manager_ = std::make_unique<InputDeviceSettingsMetricsManager>();
}

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
    mojom::CustomizationRestriction customization_restriction) {
  mice_to_observe_.insert_or_assign(device_id, customization_restriction);
}

void PeripheralCustomizationEventRewriter::StartObservingGraphicsTablet(
    int device_id,
    mojom::CustomizationRestriction customization_restriction) {
  graphics_tablets_to_observe_.insert_or_assign(device_id,
                                                customization_restriction);
}

void PeripheralCustomizationEventRewriter::StopObserving() {
  graphics_tablets_to_observe_.clear();
  mice_to_observe_.clear();
}

bool PeripheralCustomizationEventRewriter::NotifyMouseWheelEventObserving(
    const ui::MouseWheelEvent& mouse_wheel_event,
    DeviceType device_type) {
  const auto customization_restriction_iter =
      mice_to_observe_.find(mouse_wheel_event.source_device_id());
  if (customization_restriction_iter == mice_to_observe_.end()) {
    return false;
  }

  auto customization_restriction = customization_restriction_iter->second;
  if (!AreScrollWheelEventRewritesAllowed(customization_restriction)) {
    return false;
  }

  const mojom::ButtonPtr button =
      GetButtonFromMouseWheelEvent(mouse_wheel_event);
  if (!button) {
    return false;
  }

  switch (device_type) {
    case DeviceType::kMouse:
      input_device_settings_controller_->OnMouseButtonPressed(
          mouse_wheel_event.source_device_id(), *button);
      break;
    case DeviceType::kGraphicsTablet:
      input_device_settings_controller_->OnGraphicsTabletButtonPressed(
          mouse_wheel_event.source_device_id(), *button);
      break;
  }

  return true;
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

  if (mouse_event.type() != ui::EventType::kMousePressed) {
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

bool PeripheralCustomizationEventRewriter::IsButtonCustomizable(
    const ui::KeyEvent& key_event) {
  const auto iter = mice_to_observe_.find(key_event.source_device_id());
  if (iter == mice_to_observe().end()) {
    return false;
  }
  const auto customization_restriction = iter->second;
  // There are several cases for the customization restriction:
  // 1. If restriction is kAllowCustomizations, mice are allowed to observe
  // key events.
  // 2. If restriction is kAllowAlphabetKeyEventRewrites, mice are allowed to
  // observe only alphabet or punctuation events.
  // 3. If restriction is kAllowAlphabetOrNumberKeyEventRewrites, mice are
  // allowed to observe alphabet, punctuation, or number key event.
  // 4. Mice are not allowed to observe key event in other cases.
  switch (customization_restriction) {
    case mojom::CustomizationRestriction::kAllowCustomizations:
      return true;
    case mojom::CustomizationRestriction::kAllowAlphabetKeyEventRewrites:
      return IsAlphaKeyEvent(key_event);
    case mojom::CustomizationRestriction::
        kAllowAlphabetOrNumberKeyEventRewrites:
      return IsAlphaKeyEvent(key_event) || IsNumberKeyEvent(key_event);
    case mojom::CustomizationRestriction::kAllowTabEventRewrites:
      return key_event.key_code() == ui::VKEY_TAB;
    case mojom::CustomizationRestriction::kAllowFKeyRewrites:
      return key_event.key_code() >= ui::VKEY_F1 &&
             key_event.key_code() <= ui::VKEY_F15;
    case mojom::CustomizationRestriction::kDisallowCustomizations:
    case mojom::CustomizationRestriction::kDisableKeyEventRewrites:
    case mojom::CustomizationRestriction::kAllowHorizontalScrollWheelRewrites:
      return false;
  }
}

bool PeripheralCustomizationEventRewriter::NotifyKeyEventObserving(
    const ui::KeyEvent& key_event,
    DeviceType device_type) {
  if (device_type == DeviceType::kMouse && !IsButtonCustomizable(key_event)) {
    RecordMouseInvalidKeyPressed(input_device_settings_controller_, key_event);
    return false;
  }

  // Observers should only be notified on key presses.
  if (key_event.type() != ui::EventType::kKeyPressed) {
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
    std::vector<std::unique_ptr<ui::Event>>& rewritten_events) {
  std::optional<RemappingActionResult> remapping_action_result =
      GetRemappingAction(event.source_device_id(), button);
  if (!remapping_action_result) {
    return false;
  }
  auto remapping_action = remapping_action_result->remapping_action;

  if (event.type() == ui::EventType::kKeyPressed ||
      event.type() == ui::EventType::kMousePressed) {
    metrics_manager_->RecordRemappingActionWhenButtonPressed(
        *remapping_action, remapping_action_result->peripheral_kind);
  }

  auto id = event.source_device_id();
  switch (remapping_action_result->peripheral_kind) {
    case InputDeviceSettingsMetricsManager::PeripheralCustomizationMetricsType::
        kMouse:
      PR_LOG(INFO, Feature::IDS) << GetMouseSettingsLog(
          "Mouse button is pressed",
          *(input_device_settings_controller_->GetMouse(id)));
      break;
    case InputDeviceSettingsMetricsManager::PeripheralCustomizationMetricsType::
        kGraphicsTablet:
      PR_LOG(INFO, Feature::IDS) << GetGraphicsTabletSettingsLog(
          "Graphics tablet button is pressed",
          *(input_device_settings_controller_->GetGraphicsTablet(id)));
      break;
    case InputDeviceSettingsMetricsManager::PeripheralCustomizationMetricsType::
        kGraphicsTabletPen:
      PR_LOG(INFO, Feature::IDS) << GetGraphicsTabletSettingsLog(
          "Graphics tablet pen button is pressed",
          *(input_device_settings_controller_->GetGraphicsTablet(id)));
      break;
  }

  if (remapping_action->is_accelerator_action()) {
    if (event.type() == ui::EventType::kKeyPressed ||
        event.type() == ui::EventType::kMousePressed ||
        event.type() == ui::EventType::kMousewheel) {
      // Every accelerator supported by peripheral customization is not impacted
      // by the accelerator passed. Therefore, passing an empty accelerator will
      // cause no issues.
      Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
          remapping_action->get_accelerator_action(), /*accelerator=*/{});
    }

    return true;
  }

  // Get flags to release in rewriting key events.
  int flags_to_release = 0;
  auto iter =
      device_button_to_flags_.find({event.source_device_id(), button.Clone()});
  if (iter != device_button_to_flags_.end()) {
    flags_to_release = iter->second;
  }

  if (remapping_action->is_key_event()) {
    const auto& key_event = remapping_action->get_key_event();
    auto entry = FindKeyCodeEntry(key_event->vkey);
    // If no entry can be found, use the stored key_event struct.
    if (!entry) {
      rewritten_events =
          RewriteEventToKeyEvents(event, *key_event, flags_to_release);
    } else {
      rewritten_events = RewriteEventToKeyEvents(
          event,
          mojom::KeyEvent(
              entry->resulting_key_code, static_cast<int>(entry->dom_code),
              static_cast<int>(entry->dom_key), key_event->modifiers, ""),
          flags_to_release);
    }
  }

  if (remapping_action->is_static_shortcut_action()) {
    const auto static_action = remapping_action->get_static_shortcut_action();
    if (static_action == mojom::StaticShortcutAction::kDisable) {
      // Return true to discard the event.
      return true;
    }

    if (kStaticActionToMouseButtonFlag.contains(static_action)) {
      rewritten_events = RewriteEventToMouseButtonEvents(event, static_action);
    } else {
      rewritten_events = RewriteEventToKeyEvents(
          event,
          GetStaticShortcutAction(
              remapping_action->get_static_shortcut_action()),
          flags_to_release);
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

  // Clone event and remove the already remapped modifiers and use this as the
  // "source" key event for the rest of the rewriting.
  std::unique_ptr<ui::Event> original_event_with_modifiers_removed =
      CloneEvent(key_event);
  RemoveRemappedModifiers(*original_event_with_modifiers_removed);

  std::vector<std::unique_ptr<ui::Event>> rewritten_events;
  mojom::ButtonPtr button = mojom::Button::NewVkey(key_event.key_code());
  bool updated_button_map = false;
  if (RewriteEventFromButton(*original_event_with_modifiers_removed, *button,
                             rewritten_events)) {
    return DiscardEvent(continuation);
  }

  const bool event_rewritten = !rewritten_events.empty();

  // Discard all "button" type events usually from graphics tablets.
  if (!event_rewritten && key_event.key_code() >= ui::VKEY_BUTTON_0 &&
      key_event.key_code() <= ui::VKEY_BUTTON_Z) {
    return DiscardEvent(continuation);
  }

  // Add an event to our list to rewrite based on other pressed buttons.
  if (rewritten_events.empty()) {
    rewritten_events.push_back(
        std::move(original_event_with_modifiers_removed));
  }

  // If the button was released, the pressed button map must be updated before
  // applying remapped modifiers.
  const ui::Event& last_rewritten_event = *rewritten_events.back();
  if (event_rewritten &&
      (last_rewritten_event.type() == ui::EventType::kMouseReleased ||
       last_rewritten_event.type() == ui::EventType::kKeyReleased)) {
    updated_button_map = true;
    UpdatePressedButtonMap(std::move(button), key_event, rewritten_events);
  }

  // Remove flags from modifiers that are released on other devices.
  if (!event_rewritten) {
    UpdatePressedButtonMapFlags(key_event);
  }

  for (const auto& rewritten_event : rewritten_events) {
    ApplyRemappedModifiers(*rewritten_event);
  }

  if (event_rewritten && !updated_button_map) {
    UpdatePressedButtonMap(std::move(button), key_event, rewritten_events);
  }

  ui::EventDispatchDetails details;
  for (const auto& rewritten_event : rewritten_events) {
    details = SendEvent(continuation, rewritten_event.get());
  }
  return details;
}

void PeripheralCustomizationEventRewriter::UpdatePressedButtonMap(
    mojom::ButtonPtr button,
    const ui::Event& original_event,
    const std::vector<std::unique_ptr<ui::Event>>& rewritten_events) {
  // Scroll wheel events cannot affect other events modifiers since they do a
  // full press/release sequence with the one event.
  if (original_event.type() == ui::EventType::kMousewheel) {
    return;
  }

  DeviceIdButton device_id_button_key =
      DeviceIdButton{original_event.source_device_id(), std::move(button)};
  // If the button is released, the entry must be removed from the map.
  if (original_event.type() == ui::EventType::kMouseReleased ||
      original_event.type() == ui::EventType::kKeyReleased) {
    device_button_to_flags_.erase(std::move(device_id_button_key));

    // Release all modifier flags on other currently pressed buttons.
    for (const auto& rewritten_event : rewritten_events) {
      if (rewritten_event->IsKeyEvent()) {
        UpdatePressedButtonMapFlags(*rewritten_event->AsKeyEvent());
      }
    }
    return;
  }

  // For each rewritten event, combine the flags that need to be applied to
  // correctly handle the newly pressed event. This matters when pressing a
  // modifier or a key with a combo of modifiers or when holding a rewritten
  // mouse button.
  ui::EventFlags event_flags = 0;
  for (const auto& rewritten_event : rewritten_events) {
    if (!rewritten_event) {
      continue;
    }

    if (rewritten_event->IsKeyEvent()) {
      const auto& key_event = *rewritten_event->AsKeyEvent();
      event_flags |= ConvertKeyCodeToFlags(key_event.key_code());
      continue;
    }

    if (rewritten_event->IsMouseEvent()) {
      const auto& mouse_event = *rewritten_event->AsMouseEvent();
      event_flags |= mouse_event.changed_button_flags();
      continue;
    }
  }

  if (!event_flags) {
    return;
  }

  // Add the entry to the map with the flags that must be applied to other
  // events.
  device_button_to_flags_.insert_or_assign(std::move(device_id_button_key),
                                           event_flags);
}

void PeripheralCustomizationEventRewriter::UpdatePressedButtonMapFlags(
    const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::EventType::kKeyPressed) {
    return;
  }

  // Remap the released key based on modifier remappings.
  auto* settings = input_device_settings_controller_->GetKeyboardSettings(
      key_event.source_device_id());
  auto modifier_key = ConvertDomCodeToModifierKey(key_event.code());
  int key_event_characteristic_flag =
      ConvertKeyCodeToFlags(key_event.key_code());
  // Modifiers only need to be remapped now if the rewriter fix is disabled.
  if (!features::IsKeyboardRewriterFixEnabled() && settings && modifier_key) {
    auto iter = settings->modifier_remappings.find(*modifier_key);
    if (iter != settings->modifier_remappings.end()) {
      key_event_characteristic_flag = ConvertModifierKeyToFlags(iter->second);
    }
  }

  // Remove the key event characteristic flag as the key has already been
  // released and should no longer apply the flag to other pressed events.
  for (auto& [_, flag] : device_button_to_flags_) {
    flag &= ~key_event_characteristic_flag;
  }
}

ui::EventDispatchDetails
PeripheralCustomizationEventRewriter::RewriteMouseWheelEvent(
    const ui::MouseWheelEvent& mouse_wheel_event,
    const Continuation continuation) {
  auto device_type_to_observe =
      GetDeviceTypeToObserve(mouse_wheel_event.source_device_id());
  if (device_type_to_observe) {
    if (NotifyMouseWheelEventObserving(mouse_wheel_event,
                                       *device_type_to_observe)) {
      return DiscardEvent(continuation);
    }

    // Otherwise, the flags must be cleared for the remappable buttons so they
    // do not affect the application while the mouse is meant to be observed.
    std::unique_ptr<ui::Event> rewritten_event = CloneEvent(mouse_wheel_event);
    const int remappable_flags =
        GetRemappableMouseEventFlags(*device_type_to_observe);
    rewritten_event->SetFlags(rewritten_event->flags() & ~remappable_flags);
    if (rewritten_event->IsMouseEvent()) {
      auto& rewritten_mouse_event = *rewritten_event->AsMouseEvent();
      rewritten_mouse_event.set_changed_button_flags(
          rewritten_mouse_event.changed_button_flags() & ~remappable_flags);
    }
    return SendEvent(continuation, rewritten_event.get());
  }

  // Clone event and remove the already remapped modifiers and use this as the
  // "source" key event for the rest of the rewriting.
  std::unique_ptr<ui::Event> original_event_with_modifiers_removed =
      CloneEvent(mouse_wheel_event);
  RemoveRemappedModifiers(*original_event_with_modifiers_removed);

  std::vector<std::unique_ptr<ui::Event>> rewritten_events;
  mojom::ButtonPtr button = GetButtonFromMouseWheelEvent(mouse_wheel_event);
  bool updated_button_map = false;
  if (!button.is_null()) {
    if (RewriteEventFromButton(*original_event_with_modifiers_removed, *button,
                               rewritten_events)) {
      return DiscardEvent(continuation);
    }
  }

  const bool event_rewritten = !rewritten_events.empty();

  // Add an event to our list to rewrite based on other pressed buttons.
  if (!event_rewritten) {
    rewritten_events.push_back(
        std::move(original_event_with_modifiers_removed));
  }

  // If the button was released, the pressed button map must be updated before
  // applying remapped modifiers.
  const ui::Event& last_rewritten_event = *rewritten_events.back();
  if (event_rewritten &&
      (last_rewritten_event.type() == ui::EventType::kMouseReleased ||
       last_rewritten_event.type() == ui::EventType::kKeyReleased)) {
    updated_button_map = true;
    UpdatePressedButtonMap(std::move(button), mouse_wheel_event,
                           rewritten_events);
  }

  for (const auto& rewritten_event : rewritten_events) {
    ApplyRemappedModifiers(*rewritten_event);
  }

  if (event_rewritten && !updated_button_map) {
    UpdatePressedButtonMap(std::move(button), mouse_wheel_event,
                           rewritten_events);
  }

  ui::EventDispatchDetails details;
  for (const auto& rewritten_event : rewritten_events) {
    details = SendEvent(continuation, rewritten_event.get());
  }
  return details;
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
    rewritten_event->SetFlags(rewritten_event->flags() & ~remappable_flags);
    if (rewritten_event->IsMouseEvent()) {
      auto& rewritten_mouse_event = *rewritten_event->AsMouseEvent();
      rewritten_mouse_event.set_changed_button_flags(
          rewritten_mouse_event.changed_button_flags() & ~remappable_flags);
    }
    return SendEvent(continuation, rewritten_event.get());
  }

  // Clone event and remove the already remapped modifiers and use this as the
  // "source" key event for the rest of the rewriting.
  std::unique_ptr<ui::Event> original_event_with_modifiers_removed =
      CloneEvent(mouse_event);
  RemoveRemappedModifiers(*original_event_with_modifiers_removed);

  std::vector<std::unique_ptr<ui::Event>> rewritten_events;
  mojom::ButtonPtr button;
  bool updated_button_map = false;
  if (IsMouseButtonEvent(mouse_event) && mouse_event.changed_button_flags()) {
    button = GetButtonFromMouseEvent(mouse_event);
    if (RewriteEventFromButton(*original_event_with_modifiers_removed, *button,
                               rewritten_events)) {
      return DiscardEvent(continuation);
    }
  }

  const bool event_rewritten = !rewritten_events.empty();

  // Add an event to our list to rewrite based on other pressed buttons.
  if (!event_rewritten) {
    rewritten_events.push_back(
        std::move(original_event_with_modifiers_removed));
  }

  // If the button was released, the pressed button map must be updated before
  // applying remapped modifiers.
  const ui::Event& last_rewritten_event = *rewritten_events.back();
  if (event_rewritten &&
      (last_rewritten_event.type() == ui::EventType::kMouseReleased ||
       last_rewritten_event.type() == ui::EventType::kKeyReleased)) {
    updated_button_map = true;
    UpdatePressedButtonMap(std::move(button), mouse_event, rewritten_events);
  }

  for (const auto& rewritten_event : rewritten_events) {
    ApplyRemappedModifiers(*rewritten_event);
  }

  if (event_rewritten && !updated_button_map) {
    UpdatePressedButtonMap(std::move(button), mouse_event, rewritten_events);
  }

  ui::EventDispatchDetails details;
  for (const auto& rewritten_event : rewritten_events) {
    details = SendEvent(continuation, rewritten_event.get());
  }
  return details;
}

ui::EventDispatchDetails PeripheralCustomizationEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DCHECK(features::IsPeripheralCustomizationEnabled() ||
         ::features::IsShortcutCustomizationEnabled());

  if (event.IsMouseWheelEvent()) {
    return RewriteMouseWheelEvent(*event.AsMouseWheelEvent(), continuation);
  }

  if (event.IsMouseEvent()) {
    return RewriteMouseEvent(*event.AsMouseEvent(), continuation);
  }

  if (event.IsKeyEvent()) {
    return RewriteKeyEvent(*event.AsKeyEvent(), continuation);
  }

  return SendEvent(continuation, &event);
}

std::optional<PeripheralCustomizationEventRewriter::RemappingActionResult>
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

  return std::nullopt;
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
  if (modifier_flags) {
    event.SetFlags(event.flags() & ~modifier_flags);
  }
}

void PeripheralCustomizationEventRewriter::ApplyRemappedModifiers(
    ui::Event& event) {
  int flags = 0;
  for (const auto& [_, flag] : device_button_to_flags_) {
    flags |= flag;
  }
  if (flags) {
    event.SetFlags(event.flags() | flags);
  }
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
