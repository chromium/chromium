// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFERENCES_ANDROID_CHROME_SHARED_PREFERENCES_H_
#define CHROME_BROWSER_PREFERENCES_ANDROID_CHROME_SHARED_PREFERENCES_H_

#import "base/android/shared_preferences/shared_preferences_manager.h"

namespace android::shared_preferences {

// Get a SharedPreferencesManager to access SharedPreferences registered in
// ChromePreferenceKeys.java.
const base::android::SharedPreferencesManager GetChromeSharedPreferences();

}  // namespace android::shared_preferences

#endif  // CHROME_BROWSER_PREFERENCES_ANDROID_CHROME_SHARED_PREFERENCES_H_
