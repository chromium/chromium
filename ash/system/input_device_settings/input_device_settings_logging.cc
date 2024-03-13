// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_logging.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace ash {

namespace {

// Return the device type name based on the given device type.
template <typename T>
std::string_view GetDeviceTypeName() {
  if constexpr (std::is_same_v<T, mojom::Keyboard>) {
    return "Keyboard";
  } else if constexpr (std::is_same_v<T, mojom::Mouse>) {
    return "Mouse";
  } else if constexpr (std::is_same_v<T, mojom::Touchpad>) {
    return "Touchpad";
  } else if constexpr (std::is_same_v<T, mojom::PointingStick>) {
    return "PointingStick";
  } else if constexpr (std::is_same_v<T, mojom::GraphicsTablet>) {
    return "GraphicsTablet";
  }
}

// Get the general device information log.
template <typename MojomDevice>
std::string GetDeviceLog(std::string_view category,
                         const MojomDevice& device,
                         std::string_view type,
                         std::string log) {
  std::string log_for_device;
  std::string general_info =
      base::JoinString({category, "device name:", device.name, "type:", type,
                        "vid:pid:", device.device_key,
                        "device_id:", base::NumberToString(device.id)},
                       " ");
  if constexpr (std::is_same_v<MojomDevice, mojom::Mouse>) {
    general_info = base::JoinString({general_info, "customization_restriction:",
                                     base::NumberToString(static_cast<int>(
                                         device.customization_restriction))},
                                    " ");
  }
  log_for_device = base::JoinString(
      {general_info, "Current settings for", device.name, ":", log}, " ");

  return log_for_device;
}

}  // namespace

std::string_view GetBooleanString(bool value) {
  if (value) {
    return "true";
  }
  return "false";
}

std::string GetButtonLog(const mojom::ButtonRemappingPtr& button_remapping) {
  if (button_remapping->button->is_customizable_button()) {
    return base::StrCat(
        {"customizable_button_",
         base::NumberToString(static_cast<int>(
             button_remapping->button->get_customizable_button()))});
  }

  return base::StrCat({"vkey_", base::NumberToString(static_cast<int>(
                                    button_remapping->button->get_vkey()))});
}

std::string GetRemappingActionLog(
    const mojom::ButtonRemappingPtr& button_remapping) {
  if (button_remapping->remapping_action.is_null()) {
    return "default";
  } else if (button_remapping->remapping_action->is_static_shortcut_action()) {
    return base::StrCat(
        {"static_shortcut_action_", base::NumberToString(static_cast<int>(
                                        button_remapping->remapping_action
                                            ->get_static_shortcut_action()))});
  } else if (button_remapping->remapping_action->is_key_event()) {
    return base::JoinString(
        {"key_event:", "domcode:",
         base::NumberToString(static_cast<int>(
             button_remapping->remapping_action->get_key_event()->dom_code)),
         "dom_key:",
         base::NumberToString(static_cast<int>(
             button_remapping->remapping_action->get_key_event()->dom_key)),
         "vkey:",
         base::NumberToString(static_cast<int>(
             button_remapping->remapping_action->get_key_event()->vkey)),
         "modifiers:",
         base::NumberToString(static_cast<int>(
             button_remapping->remapping_action->get_key_event()->modifiers)),
         "key_display:",
         button_remapping->remapping_action->get_key_event()->key_display},
        " ");
  } else {
    return base::JoinString(
        {"accelerator_action:",
         base::NumberToString(static_cast<int>(
             button_remapping->remapping_action->get_accelerator_action()))},
        " ");
  }
}

std::string GetButtonRemappingsLog(
    const std::vector<mojom::ButtonRemappingPtr>& button_remappings) {
  std::string button_remapping_log;
  for (const auto& remapping : button_remappings) {
    std::string button = GetButtonLog(remapping);
    std::string remapping_action = GetRemappingActionLog(remapping);
    button_remapping_log = base::JoinString(
        {button_remapping_log, "button_remapping:", "name:", remapping->name,
         "button:", button, "remapping_action:", remapping_action},
        " ");
  }
  return button_remapping_log;
}

std::string GetKeyboardSettingsLogList(const mojom::Keyboard& keyboard) {
  if (!keyboard.settings.get()) {
    return "";
  }
  std::string_view top_row_are_fkeys =
      GetBooleanString(keyboard.settings->top_row_are_fkeys);
  std::string_view suppress_meta_fkey_rewrites =
      GetBooleanString(keyboard.settings->suppress_meta_fkey_rewrites);
  std::string f11;
  std::string f12;
  if (keyboard.settings->f11) {
    f11 =
        base::NumberToString(static_cast<int>(keyboard.settings->f11.value()));
  }
  if (keyboard.settings->f12) {
    f12 =
        base::NumberToString(static_cast<int>(keyboard.settings->f12.value()));
  }

  std::string six_pack_key_remappings;
  if (keyboard.settings->six_pack_key_remappings) {
    six_pack_key_remappings = base::JoinString(
        {"home:",
         base::NumberToString(static_cast<int>(
             keyboard.settings->six_pack_key_remappings->home)),
         "page_up:",
         base::NumberToString(static_cast<int>(
             keyboard.settings->six_pack_key_remappings->page_up)),
         "page_down:",
         base::NumberToString(static_cast<int>(
             keyboard.settings->six_pack_key_remappings->page_down)),
         "del:",
         base::NumberToString(
             static_cast<int>(keyboard.settings->six_pack_key_remappings->del)),
         "insert:",
         base::NumberToString(static_cast<int>(
             keyboard.settings->six_pack_key_remappings->insert)),
         "end:",
         base::NumberToString(static_cast<int>(
             keyboard.settings->six_pack_key_remappings->end))},
        " ");
  }

  return base::JoinString(
      {"top_row_are_fkeys:", top_row_are_fkeys,
       "six_pack_key_remappings:", six_pack_key_remappings,
       "suppress_meta_fkey_rewrites:", suppress_meta_fkey_rewrites, "f11:", f11,
       "f12:", f12},
      " ");
}

std::string GetMouseSettingsLogList(const mojom::Mouse& mouse) {
  std::string_view swap_right = GetBooleanString(mouse.settings->swap_right);
  std::string_view reverse_scrolling =
      GetBooleanString(mouse.settings->reverse_scrolling);
  std::string_view acceleration_enabled =
      GetBooleanString(mouse.settings->acceleration_enabled);
  std::string_view scroll_acceleration =
      GetBooleanString(mouse.settings->scroll_acceleration);
  std::string sensitivity = base::NumberToString(mouse.settings->sensitivity);
  std::string scroll_sensitivity =
      base::NumberToString(mouse.settings->scroll_sensitivity);

  return base::JoinString(
      {"swap_right:", swap_right, "sensitivity:", sensitivity,
       "reverse_scrolling:", reverse_scrolling, "acceleration_enabled:",
       acceleration_enabled, "scroll_sensitivity:", scroll_sensitivity,
       "scroll_acceleration:", scroll_acceleration},
      " ");
}

std::string GetPointingStickSettingsLogList(
    const mojom::PointingStick& pointing_stick) {
  std::string_view swap_right =
      GetBooleanString(pointing_stick.settings->swap_right);
  std::string_view acceleration_enabled =
      GetBooleanString(pointing_stick.settings->acceleration_enabled);
  std::string sensitivity =
      base::NumberToString(pointing_stick.settings->sensitivity);

  return base::JoinString(
      {"swap_right:", swap_right, "sensitivity:", sensitivity,
       "acceleration_enabled:", acceleration_enabled},
      " ");
}

std::string GetTouchpadSettingsLogList(const mojom::Touchpad& touchpad) {
  std::string_view tap_to_click_enabled =
      GetBooleanString(touchpad.settings->tap_to_click_enabled);
  std::string_view three_finger_click_enabled =
      GetBooleanString(touchpad.settings->three_finger_click_enabled);
  std::string_view tap_dragging_enabled =
      GetBooleanString(touchpad.settings->tap_dragging_enabled);
  std::string_view reverse_scrolling =
      GetBooleanString(touchpad.settings->reverse_scrolling);
  std::string_view acceleration_enabled =
      GetBooleanString(touchpad.settings->acceleration_enabled);
  std::string_view scroll_acceleration =
      GetBooleanString(touchpad.settings->scroll_acceleration);
  std::string sensitivity =
      base::NumberToString(touchpad.settings->sensitivity);
  std::string haptic_sensitivity =
      base::NumberToString(touchpad.settings->haptic_sensitivity);
  std::string haptic_enabled =
      base::NumberToString(touchpad.settings->haptic_enabled);
  std::string scroll_sensitivity =
      base::NumberToString(touchpad.settings->scroll_sensitivity);
  std::string simulate_right_click = base::NumberToString(
      static_cast<int>(touchpad.settings->simulate_right_click));

  return base::JoinString({"tap_to_click_enabled:",
                           tap_to_click_enabled,
                           "three_finger_click_enabled:",
                           three_finger_click_enabled,
                           "tap_dragging_enabled:",
                           tap_dragging_enabled,
                           "sensitivity:",
                           sensitivity,
                           "haptic_sensitivity:",
                           haptic_sensitivity,
                           "haptic_enabled:",
                           haptic_enabled,
                           "reverse_scrolling:",
                           reverse_scrolling,
                           "acceleration_enabled:",
                           acceleration_enabled,
                           "scroll_sensitivity:",
                           scroll_sensitivity,
                           "scroll_acceleration:",
                           scroll_acceleration,
                           "simulate_right_click:",
                           simulate_right_click},
                          " ");
}

std::string GetGraphicsTabletSettingsLog(
    std::string_view category,
    const mojom::GraphicsTablet& graphics_tablet) {
  return GetDeviceLog(
      category, graphics_tablet, GetDeviceTypeName<mojom::GraphicsTablet>(),
      base::JoinString({GetButtonRemappingsLog(
                            graphics_tablet.settings->tablet_button_remappings),
                        GetButtonRemappingsLog(
                            graphics_tablet.settings->pen_button_remappings)},
                       " "));
}

std::string GetKeyboardSettingsLog(std::string_view category,
                                   const mojom::Keyboard& keyboard) {
  return GetDeviceLog(category, keyboard, GetDeviceTypeName<mojom::Keyboard>(),
                      GetKeyboardSettingsLogList(keyboard));
}

std::string GetMouseSettingsLog(std::string_view category,
                                const mojom::Mouse& mouse) {
  return GetDeviceLog(
      category, mouse, GetDeviceTypeName<mojom::Mouse>(),
      base::JoinString(
          {GetMouseSettingsLogList(mouse),
           GetButtonRemappingsLog(mouse.settings->button_remappings)},
          " "));
}

std::string GetPointingStickSettingsLog(
    std::string_view category,
    const mojom::PointingStick& pointing_stick) {
  return GetDeviceLog(category, pointing_stick,
                      GetDeviceTypeName<mojom::PointingStick>(),
                      GetPointingStickSettingsLogList(pointing_stick));
}

std::string GetTouchpadSettingsLog(std::string_view category,
                                   const mojom::Touchpad& touchpad) {
  return GetDeviceLog(category, touchpad, GetDeviceTypeName<mojom::Touchpad>(),
                      GetTouchpadSettingsLogList(touchpad));
}

}  // namespace ash
