// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFETY_HUB_ANDROID_PREFS_H_
#define CHROME_BROWSER_SAFETY_HUB_ANDROID_PREFS_H_

class PrefRegistrySimple;

namespace safety_hub_prefs {

void RegisterSafetyHubAndroidProfilePrefs(PrefRegistrySimple* registry);

}  // namespace safety_hub_prefs

#endif  // CHROME_BROWSER_SAFETY_HUB_ANDROID_PREFS_H_
