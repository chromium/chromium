// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

TabAndroid* ToTabAndroidOrNull(TabInterface* tab_interface) {
  if (!tab_interface) {
    LOG(WARNING) << "Attempting to convert a nullptr to a TabAndroid*.";
    return nullptr;
  }
  // The weak ptr for TabAndroid and TabInterfaceAndroid both point to
  // TabAndroid so we can use that to cast back to a TabAndroid* safely.
  auto weak_tab_android = tab_interface->GetWeakPtr();
  if (!weak_tab_android) {
    LOG(WARNING) << "Underlying TabAndroid already destroyed.";
    return nullptr;
  }
  return static_cast<TabAndroid*>(weak_tab_android.get());
}

TabAndroid* ToTabAndroidChecked(TabInterface* tab_interface) {
  CHECK(tab_interface);
  auto weak_tab_android = tab_interface->GetWeakPtr();
  CHECK(weak_tab_android);
  return static_cast<TabAndroid*>(weak_tab_android.get());
}

const TabAndroid* ToTabAndroidChecked(const TabInterface* tab_interface) {
  CHECK(tab_interface);
  auto weak_tab_android = tab_interface->GetHandle().Get()->GetWeakPtr();
  CHECK(weak_tab_android);
  return static_cast<const TabAndroid*>(weak_tab_android.get());
}

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_CONVERSIONS_H_
