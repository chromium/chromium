// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

TabAndroid* ToTabAndroidOrNull(TabInterface* tab_interface) {
  return static_cast<TabAndroid*>(tab_interface);
}

TabAndroid* ToTabAndroidChecked(TabInterface* tab_interface) {
  CHECK(tab_interface);
  return static_cast<TabAndroid*>(tab_interface);
}

const TabAndroid* ToTabAndroidChecked(const TabInterface* tab_interface) {
  CHECK(tab_interface);
  return static_cast<const TabAndroid*>(tab_interface);
}

}  // namespace tabs
