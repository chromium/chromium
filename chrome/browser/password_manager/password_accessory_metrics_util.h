// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_METRICS_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_METRICS_UTIL_H_

namespace metrics {

// Used to record keyboard accessory bar impressions bucketed by content.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the enum
// in enums.xml. A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessoryBarContents {
  NO_CONTENTS = 0,   // Increased if none of the other buckets increases.
  ANY_CONTENTS = 1,  // Increased if least one of the other buckets increases.
  WITH_TABS = 2,     // Increased if there are tabs in the opened accessory.
  WITH_ACTIONS = 3,  // Increased if there are actions in the opened accessory.
  WITH_AUTOFILL_SUGGESTIONS = 4,  // Increased for autofill suggestions.
  COUNT,
};

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
  TOUCH_TO_FILL_INFO = 4,
  COUNT,
};

}  // namespace metrics

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_METRICS_UTIL_H_
