// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/feature_utils.h"

#include "build/build_config.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/tab_group_sync/utils_jni_headers/TabGroupSyncFeatures_jni.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/pref_names.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tab_groups {

#if BUILDFLAG(IS_ANDROID)
// static
jboolean JNI_TabGroupSyncFeatures_IsTabGroupSyncEnabled(JNIEnv* env,
                                                        Profile* profile) {
  DCHECK(profile);
  return IsTabGroupSyncEnabled(profile->GetPrefs());
}
#endif  // BUILDFLAG(IS_ANDROID)

bool IsTabGroupSyncEnabled(PrefService* pref_service) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(tab_groups::kAndroidTabGroupStableIds)) {
    return false;
  }

  // Enabled the feature if both of the following condition is true:
  // 1. If kTabGroupSyncAndroid is enabled, or kSyncableTabGroups is on.
  // 2. And kTabGroupSyncForceOff is disabled.
  // kTabGroupSyncForceOff will turn off the feature on the current device, so
  // tab groups will not be synced.
  if (base::FeatureList::IsEnabled(tab_groups::kTabGroupSyncForceOff)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(tab_groups::kTabGroupSyncAndroid)) {
    // The user is in an experiment group that enables the feature, push
    // kSyncableTabGroups preference to other devices so that the feature can
    // work on those devices too for the same user..
    pref_service->SetBoolean(tab_groups::prefs::kSyncableTabGroups, true);
    return true;
  }

  // If kSyncableTabGroups is true, the feature is enabled for the user on
  // another device through experiments. Enable the feature on the current
  // device too.
  return pref_service->GetBoolean(tab_groups::prefs::kSyncableTabGroups);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace tab_groups
