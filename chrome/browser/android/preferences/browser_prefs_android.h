// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_BROWSER_PREFS_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_BROWSER_PREFS_ANDROID_H_

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace android {

// Register all prefs that will be used via the local state PrefService.
void RegisterPrefs(PrefRegistrySimple* registry);

// Register all prefs that will be used via a PrefService attached to a user
// Profile on Android.
void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_BROWSER_PREFS_ANDROID_H_
