// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/stl_util.h"
#include "chrome/android/chrome_jni_headers/ChromeBackupAgent_jni.h"
#include "chrome/browser/android/chrome_backup_agent.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace {

const char* backed_up_preferences_[] = {
    data_reduction_proxy::prefs::kDataSaverEnabled,
    syncer::prefs::kSyncFirstSetupComplete,
    syncer::prefs::kSyncKeepEverythingSynced,
    syncer::prefs::kSyncAutofillProfile,
    syncer::prefs::kSyncAutofillWallet,
    syncer::prefs::kSyncAutofillWalletMetadata,
    syncer::prefs::kSyncAutofill,
    syncer::prefs::kSyncBookmarks,
    syncer::prefs::kSyncDeviceInfo,
    syncer::prefs::kSyncFaviconImages,
    syncer::prefs::kSyncFaviconTracking,
    syncer::prefs::kSyncHistoryDeleteDirectives,
    syncer::prefs::kSyncPasswords,
    syncer::prefs::kSyncPreferences,
    syncer::prefs::kSyncPriorityPreferences,
    syncer::prefs::kSyncRequested,
    syncer::prefs::kSyncSessions,
    syncer::prefs::kSyncSupervisedUserSettings,
    syncer::prefs::kSyncSupervisedUserSharedSettings,
    syncer::prefs::kSyncSupervisedUserWhitelists,
    syncer::prefs::kSyncTabs,
    syncer::prefs::kSyncTypedUrls,
};

}  // namespace

static base::android::ScopedJavaLocalRef<jobjectArray>
JNI_ChromeBackupAgent_GetBoolBackupNames(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return base::android::ToJavaArrayOfStrings(env,
                                             android::GetBackupPrefNames());
}

static base::android::ScopedJavaLocalRef<jbooleanArray>
JNI_ChromeBackupAgent_GetBoolBackupValues(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  PrefService* prefs = ProfileManager::GetLastUsedProfile()->GetPrefs();
  constexpr int pref_count = base::size(backed_up_preferences_);
  jboolean values[pref_count];

  for (int i = 0; i < pref_count; i++) {
    values[i] = prefs->GetBoolean(backed_up_preferences_[i]);
  }
  jbooleanArray array = env->NewBooleanArray(pref_count);
  env->SetBooleanArrayRegion(array, 0, pref_count, values);
  return base::android::ScopedJavaLocalRef<jbooleanArray>(env, array);
}

static void JNI_ChromeBackupAgent_SetBoolBackupPrefs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobjectArray>& names,
    const base::android::JavaParamRef<jbooleanArray>& values) {
  std::vector<std::string> pref_names;
  base::android::AppendJavaStringArrayToStringVector(env, names, &pref_names);
  std::vector<bool> pref_values;
  JavaBooleanArrayToBoolVector(env, values, &pref_values);
  std::unordered_set<std::string> valid_prefs(
      std::begin(backed_up_preferences_), std::end(backed_up_preferences_));

  PrefService* prefs = ProfileManager::GetLastUsedProfile()->GetPrefs();
  for (unsigned int i = 0; i < pref_names.size(); i++) {
    if (valid_prefs.count(pref_names[i])) {
      prefs->SetBoolean(pref_names[i], pref_values[i]);
    }
  }
  prefs->CommitPendingWrite();
}

namespace android {

std::vector<std::string> GetBackupPrefNames() {
  return std::vector<std::string>(std::begin(backed_up_preferences_),
                                  std::end(backed_up_preferences_));
}

base::android::ScopedJavaLocalRef<jobjectArray> GetBoolBackupNamesForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return JNI_ChromeBackupAgent_GetBoolBackupNames(env, jcaller);
}

base::android::ScopedJavaLocalRef<jbooleanArray> GetBoolBackupValuesForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return JNI_ChromeBackupAgent_GetBoolBackupValues(env, jcaller);
}

void SetBoolBackupPrefsForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobjectArray>& names,
    const base::android::JavaParamRef<jbooleanArray>& values) {
  JNI_ChromeBackupAgent_SetBoolBackupPrefs(env, jcaller, names, values);
}

}  //  namespace android
