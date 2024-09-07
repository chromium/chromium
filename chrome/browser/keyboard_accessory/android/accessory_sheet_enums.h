// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ACCESSORY_SHEET_ENUMS_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ACCESSORY_SHEET_ENUMS_H_

namespace autofill {

// Describes the different types of accessory sheets.
// Used to record metrics specific to a tab types (e.g. passwords, payments).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the suffix
// AccessorySheetType in histogram.xml. A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessoryTabType {
  ALL = 0,
  PASSWORDS = 1,
  CREDIT_CARDS = 2,
  ADDRESSES = 3,
  OBSOLETE_TOUCH_TO_FILL = 4,
  COUNT,
};

// Describes possible actions in the keyboard accessory and its sheets. Used to
// distinguish specific actions and links.
// Additionally, they are used to record metrics for the associated action.
// Therefore, entries should not be renumbered and numeric values should never
// be reused. Must be kept in sync with the enum in enums.xml. A java IntDef@ is
// generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessoryAction {
  GENERATE_PASSWORD_AUTOMATIC = 0,
  MANAGE_PASSWORDS = 1,
  AUTOFILL_SUGGESTION = 2,
  MANAGE_CREDIT_CARDS = 3,
  MANAGE_ADDRESSES = 4,
  GENERATE_PASSWORD_MANUAL = 5,
  TOGGLE_SAVE_PASSWORDS = 6,
  USE_OTHER_PASSWORD = 7,
  CREDMAN_CONDITIONAL_UI_REENTRY = 8,
  CROSS_DEVICE_PASSKEY = 9,
  CREATE_PLUS_ADDRESS_FROM_ADDRESS_SHEET = 10,
  SELECT_PLUS_ADDRESS_FROM_ADDRESS_SHEET = 11,
  MANAGE_PLUS_ADDRESS_FROM_ADDRESS_SHEET = 12,
  CREATE_PLUS_ADDRESS_FROM_PASSWORD_SHEET = 13,
  SELECT_PLUS_ADDRESS_FROM_PASSWORD_SHEET = 14,
  MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET = 15,
  COUNT,
};

// Used to record metrics for accessory toggles. Entries should not be
// renumbered and numeric values should never be reused. Must be kept in sync
// with the enum in enums.xml. A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessoryToggleType {
  SAVE_PASSWORDS_TOGGLE_ON = 0,
  SAVE_PASSWORDS_TOGGLE_OFF = 1,
  COUNT,
};

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ACCESSORY_SHEET_ENUMS_H_
