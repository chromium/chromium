// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_interface_holder_android.h"

#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"

TabInterfaceHolderAndroid::TabInterfaceHolderAndroid(TabAndroid* tab_android)
    : weak_tab_android_(tab_android->GetTabAndroidWeakPtr()) {}

TabInterfaceHolderAndroid::~TabInterfaceHolderAndroid() = default;

tabs::TabInterface* TabInterfaceHolderAndroid::GetTabInterface() {
  CHECK(weak_tab_android_);
  return weak_tab_android_.get();
}
