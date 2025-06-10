// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/feature_utils.h"

#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/utils_jni_headers/TabGroupSyncFeatures_jni.h"
#include "components/saved_tab_groups/public/pref_names.h"
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
  // Clear the legacy synced boolean pref to its default value (false) that
  // enables syncing across devices even when the feature isn't enabled on the
  // current device but is enabled on one of the remote devices. We will
  // deprecate this after a milestone.
  pref_service->ClearPref(tab_groups::prefs::kSyncableTabGroups);

  return base::FeatureList::IsEnabled(tab_groups::kTabGroupSyncAndroid);
#else
  return IsTabGroupSyncServiceDesktopMigrationEnabled();
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace tab_groups
