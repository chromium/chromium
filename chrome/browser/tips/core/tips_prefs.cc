// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/core/tips_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

#if BUILDFLAG(IS_ANDROID)

namespace notifications::tips::prefs {

// LINT.IfChange(TipsShownPrefs)
const char kAndroidTipNotificationShownESB[] =
    "android.tips.notifications.esb_shown";
const char kAndroidTipNotificationShownQuickDelete[] =
    "android.tips.notifications.quick_delete_shown";
const char kAndroidTipNotificationShownLens[] =
    "android.tips.notifications.lens_shown";
const char kAndroidTipNotificationShownBottomOmnibox[] =
    "android.tips.notifications.bottom_omnibox_shown";
const char kAndroidTipNotificationShownPasswordAutofill[] =
    "android.tips.notifications.password_autofill_shown";
const char kAndroidTipNotificationShownSignin[] =
    "android.tips.notifications.signin_shown";
const char kAndroidTipNotificationShownCreateTabGroups[] =
    "android.tips.notifications.create_tab_group_shown";
const char kAndroidTipNotificationShownCustomizeMVT[] =
    "android.tips.notifications.customize_mvt_shown";
const char kAndroidTipNotificationShownRecentTabs[] =
    "android.tips.notifications.recent_tabs_shown";
// LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/notifications/tips/TipsUtils.java:TipsShownPrefs)

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kAndroidTipNotificationShownESB, false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownQuickDelete, false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownLens, false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownBottomOmnibox,
                                false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownPasswordAutofill,
                                false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownSignin, false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownCreateTabGroups,
                                false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownCustomizeMVT,
                                false);
  registry->RegisterBooleanPref(kAndroidTipNotificationShownRecentTabs, false);
}

}  // namespace notifications::tips::prefs

#endif  // BUILDFLAG(IS_ANDROID)
