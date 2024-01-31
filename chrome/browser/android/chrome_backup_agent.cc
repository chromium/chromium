// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_backup_agent.h"

#include <iterator>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_string_value_serializer.h"
#include "chrome/android/chrome_jni_headers/ChromeBackupAgentImpl_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"

namespace {

static_assert(49 == syncer::GetNumModelTypes(),
              "If the new type has a corresponding pref, add it here");
const char* const kBackedUpBoolPreferences[] = {
    syncer::prefs::internal::kSyncKeepEverythingSynced,
    syncer::prefs::internal::kSyncApps,
    syncer::prefs::internal::kSyncAutofill,
    syncer::prefs::internal::kSyncBookmarks,
    syncer::prefs::internal::kSyncHistory,
    syncer::prefs::internal::kSyncPasswords,
    syncer::prefs::internal::kSyncPayments,
    syncer::prefs::internal::kSyncPreferences,
    syncer::prefs::internal::kSyncReadingList,
    syncer::prefs::internal::kSyncSavedTabGroups,
    syncer::prefs::internal::kSyncSharedTabGroupData,
    syncer::prefs::internal::kSyncTabs,
};

}  // namespace

static base::android::ScopedJavaLocalRef<jobjectArray>
JNI_ChromeBackupAgentImpl_GetBoolBackupNames(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return base::android::ToJavaArrayOfStrings(env,
                                             android::GetBackupBoolPrefNames());
}

static base::android::ScopedJavaLocalRef<jbooleanArray>
JNI_ChromeBackupAgentImpl_GetBoolBackupValues(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  PrefService* prefs = ProfileManager::GetLastUsedProfile()->GetPrefs();
  constexpr int pref_count = std::size(kBackedUpBoolPreferences);
  jboolean values[pref_count];

  for (int i = 0; i < pref_count; i++) {
    values[i] = prefs->GetBoolean(kBackedUpBoolPreferences[i]);
  }
  jbooleanArray array = env->NewBooleanArray(pref_count);
  env->SetBooleanArrayRegion(array, 0, pref_count, values);
  return base::android::ScopedJavaLocalRef<jbooleanArray>(env, array);
}

static void JNI_ChromeBackupAgentImpl_SetBoolBackupPrefs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobjectArray>& names,
    const base::android::JavaParamRef<jbooleanArray>& values) {
  std::vector<std::string> pref_names;
  base::android::AppendJavaStringArrayToStringVector(env, names, &pref_names);
  std::vector<bool> pref_values;
  base::android::JavaBooleanArrayToBoolVector(env, values, &pref_values);
  std::unordered_set<std::string> valid_prefs(
      std::begin(kBackedUpBoolPreferences), std::end(kBackedUpBoolPreferences));

  PrefService* prefs = ProfileManager::GetLastUsedProfile()->GetPrefs();
  for (unsigned int i = 0; i < pref_names.size(); i++) {
    if (valid_prefs.count(pref_names[i])) {
      prefs->SetBoolean(pref_names[i], pref_values[i]);
    }
  }
  prefs->CommitPendingWrite();
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_ChromeBackupAgentImpl_GetAccountSettingsBackupName(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return base::android::ConvertUTF8ToJavaString(
      env, syncer::prefs::internal::kSelectedTypesPerAccount);
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_ChromeBackupAgentImpl_GetAccountSettingsBackupValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  PrefService* prefs = ProfileManager::GetLastUsedProfile()->GetPrefs();
  const base::Value::Dict& account_settings =
      prefs->GetDict(syncer::prefs::internal::kSelectedTypesPerAccount);

  std::string serialized_dict;
  JSONStringValueSerializer serializer(&serialized_dict);
  const bool serializer_result = serializer.Serialize(account_settings);
  CHECK(serializer_result);
  return base::android::ConvertUTF8ToJavaString(env, serialized_dict);
}

namespace android {

std::vector<std::string> GetBackupBoolPrefNames() {
  return std::vector<std::string>(std::begin(kBackedUpBoolPreferences),
                                  std::end(kBackedUpBoolPreferences));
}

std::string GetBackupAccountSettingsPrefName() {
  return syncer::prefs::internal::kSelectedTypesPerAccount;
}

base::android::ScopedJavaLocalRef<jobjectArray> GetBoolBackupNamesForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return JNI_ChromeBackupAgentImpl_GetBoolBackupNames(env, jcaller);
}

base::android::ScopedJavaLocalRef<jbooleanArray> GetBoolBackupValuesForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return JNI_ChromeBackupAgentImpl_GetBoolBackupValues(env, jcaller);
}

void SetBoolBackupPrefsForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobjectArray>& names,
    const base::android::JavaParamRef<jbooleanArray>& values) {
  JNI_ChromeBackupAgentImpl_SetBoolBackupPrefs(env, jcaller, names, values);
}

base::android::ScopedJavaLocalRef<jstring>
GetAccountSettingsBackupNameForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return JNI_ChromeBackupAgentImpl_GetAccountSettingsBackupName(env, jcaller);
}

base::android::ScopedJavaLocalRef<jstring>
GetAccountSettingsBackupValueForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return JNI_ChromeBackupAgentImpl_GetAccountSettingsBackupValue(env, jcaller);
}

}  //  namespace android
