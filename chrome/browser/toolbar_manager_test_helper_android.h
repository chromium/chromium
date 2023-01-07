// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOOLBAR_MANAGER_TEST_HELPER_ANDROID_H_
#define CHROME_BROWSER_TOOLBAR_MANAGER_TEST_HELPER_ANDROID_H_

// Utilities that interface with Java to configure ToolbarManager properly in
// browser testing on Android.

namespace toolbar_manager {

// Skips recreating the Android activity when homepage settings are changed.
// This happens when the feature chrome::android::kStartSurfaceAndroid is
// enabled.
void setSkipRecreateForTesting(bool skipRecreating);

}  // namespace toolbar_manager

#endif  // CHROME_BROWSER_TOOLBAR_MANAGER_TEST_HELPER_ANDROID_H_
