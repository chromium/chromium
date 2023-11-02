// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_CONSTANTS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_CONSTANTS_H_

namespace apps {

// Restricts the amount of apps displayed to the user without the need of a
// ScrollView.
constexpr int kMaxAppResults = 3;

// Identifier for the Intent Chip In-Product Help feature used event.
extern const char kIntentChipOpensAppEvent[];

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_CONSTANTS_H_
