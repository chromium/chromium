// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/virtual_keyboard_log_source.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "ash/shell.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace system_logs {

namespace {

std::string LogEntryForBooleanField(const std::string& field_name, bool value) {
  return base::StrCat({field_name, ": ", base::NumberToString(value), "\n"});
}

}  // namespace

VirtualKeyboardLogSource::VirtualKeyboardLogSource()
    : SystemLogsSource("VirtualKeyboard") {}

void VirtualKeyboardLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  keyboard::KeyboardUIController* keyboard_ui_controller =
      keyboard::KeyboardUIController::Get();
  ash::VirtualKeyboardController* virtual_keyboard_controller =
      ash::Shell::Get()->keyboard_controller()->virtual_keyboard_controller();

  std::string log_data;

  int touchscreen_count = 1;
  for (const ui::InputDevice& device :
       virtual_keyboard_controller->GetTouchscreens()) {
    const std::string touchscreen_count_converted =
        base::NumberToString(touchscreen_count);
    log_data += base::StrCat({"Touchscreen ", touchscreen_count_converted,
                              " Product ID"}) +
                ": " + base::StringPrintf("0x%04x", device.product_id) + "\n";
    log_data += base::StrCat({"Touchscreen ", touchscreen_count_converted,
                              " Vendor ID"}) +
                ": " + base::StringPrintf("0x%04x", device.vendor_id) + "\n";
    ++touchscreen_count;
  }

  log_data +=
      "Internal Keyboard Name: " +
      (virtual_keyboard_controller->GetInternalKeyboardName()
           ? virtual_keyboard_controller->GetInternalKeyboardName().value()
           : "No Internal Keyboard Detected") +
      "\n";
  log_data += "Is Internal Keyboard Ignored: " +
              base::NumberToString(
                  virtual_keyboard_controller->IsInternalKeyboardIgnored()) +
              "\n";

  int external_keyboard_count = 1;
  std::vector<ui::InputDevice> external_keyboards =
      virtual_keyboard_controller->GetExternalKeyboards();
  if (external_keyboards.size() == 0) {
    log_data += "No External Keyboard Detected\n";
  }
  for (const ui::InputDevice& device : external_keyboards) {
    const std::string external_keyboard_count_converted =
        base::NumberToString(external_keyboard_count);
    log_data +=
        base::StrCat({"External Keyboard ", external_keyboard_count_converted,
                      " Product ID"}) +
        ": " + base::StringPrintf("0x%04x", device.product_id) + "\n";
    log_data +=
        base::StrCat({"External Keyboard ", external_keyboard_count_converted,
                      " Vendor ID"}) +
        ": " + base::StringPrintf("0x%04x", device.vendor_id) + "\n";
    ++external_keyboard_count;
  }

  log_data += LogEntryForBooleanField(
      "Is External Keyboard Ignored: ",
      virtual_keyboard_controller->IsExternalKeyboardIgnored());
  log_data += LogEntryForBooleanField(
      "kPolicyEnabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kPolicyEnabled));
  log_data += LogEntryForBooleanField(
      "kPolicyDisabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kPolicyDisabled));
  log_data += LogEntryForBooleanField(
      "kAndroidDisabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kAndroidDisabled));
  log_data += LogEntryForBooleanField(
      "kExtensionEnabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kExtensionEnabled));
  log_data += LogEntryForBooleanField(
      "kExtensionDisabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kExtensionDisabled));
  log_data += LogEntryForBooleanField(
      "kAccessibilityEnabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kAccessibilityEnabled));
  log_data += LogEntryForBooleanField(
      "kShelfEnabled Flag: ", keyboard_ui_controller->IsEnableFlagSet(
                                  keyboard::KeyboardEnableFlag::kShelfEnabled));
  log_data += LogEntryForBooleanField(
      "kTouchEnabled Flag: ", keyboard_ui_controller->IsEnableFlagSet(
                                  keyboard::KeyboardEnableFlag::kTouchEnabled));
  log_data += LogEntryForBooleanField(
      "kCommandLineEnabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kCommandLineEnabled));
  log_data += LogEntryForBooleanField(
      "kCommandLineDisabled Flag: ",
      keyboard_ui_controller->IsEnableFlagSet(
          keyboard::KeyboardEnableFlag::kCommandLineDisabled));
  response->emplace("virtual_keyboard", log_data);

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
