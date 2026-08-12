// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_

#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

// Safely converts a TabInterface* to a TabAndroid*.
// Returns nullptr if `tab_interface` is nullptr.
TabAndroid* ToTabAndroidOrNull(TabInterface* tab_interface);

// Converts a TabInterface* to a TabAndroid*.
// Crashes if `tab_interface` is nullptr.
TabAndroid* ToTabAndroidChecked(TabInterface* tab_interface);
const TabAndroid* ToTabAndroidChecked(const TabInterface* tab_interface);

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_
