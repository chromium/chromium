// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_METRICS_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_METRICS_UTIL_H_

namespace metrics {

// Used to record why and how often accessory sheets were opened and closed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the enum
// in enums.xml. A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessorySheetTrigger {
  ANY_CLOSE = 0,     // Increased for every closure - manual or not.
  MANUAL_CLOSE = 1,  // Increased for every user-triggered closure.
  MANUAL_OPEN = 2,   // Increased for every user-triggered opening.
  COUNT,
};

// Used to record which type of suggestion was selected.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the enum
// in enums.xml. A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessorySuggestionType {
  USERNAME = 0,
  PASSWORD = 1,
  PAYMENT_INFO = 2,
  ADDRESS_INFO = 3,
  OBSOLETE_TOUCH_TO_FILL_INFO = 4,
  COUNT,
};

}  // namespace metrics

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_METRICS_UTIL_H_
