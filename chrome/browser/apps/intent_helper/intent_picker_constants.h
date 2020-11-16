// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_CONSTANTS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_CONSTANTS_H_

namespace apps {

extern const char kUseBrowserForLink[];

// Restricts the amount of apps displayed to the user without the need of a
// ScrollView.
enum { kMaxAppResults = 3 };

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_CONSTANTS_H_
