// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

// Safely converts a TabInterface* to a TabAndroid*. This method is valid
// for inputs with a concrete type of TabInterfaceAndroid* or TabAndroid*.
// This will return nullptr if the `tab_interface` has outlived the TabAndroid*.
// There is an overhead incurred with this conversion.
TabAndroid* ToTabAndroidOrNull(TabInterface* tab_interface);

// The methods convert a TabInterface* to a TabAndroid*. These methods are valid
// for inputs with a concrete type of TabInterfaceAndroid* or TabAndroid*.
// This will crash if the `tab_interface` has outlived the TabAndroid*.
TabAndroid* ToTabAndroidChecked(TabInterface* tab_interface);
const TabAndroid* ToTabAndroidChecked(const TabInterface* tab_interface);

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_
