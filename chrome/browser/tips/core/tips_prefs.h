// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_PREFS_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_PREFS_H_

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_ANDROID)

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace notifications::tips::prefs {

// Boolean prefs indicating whether a tip notification has already been shown.
extern const char kAndroidTipNotificationShownESB[];
extern const char kAndroidTipNotificationShownQuickDelete[];
extern const char kAndroidTipNotificationShownLens[];
extern const char kAndroidTipNotificationShownBottomOmnibox[];
extern const char kAndroidTipNotificationShownPasswordAutofill[];
extern const char kAndroidTipNotificationShownSignin[];
extern const char kAndroidTipNotificationShownCreateTabGroups[];
extern const char kAndroidTipNotificationShownCustomizeMVT[];
extern const char kAndroidTipNotificationShownRecentTabs[];

// Registers the profile preferences for the Tips module.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace notifications::tips::prefs

#endif  // BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_PREFS_H_
