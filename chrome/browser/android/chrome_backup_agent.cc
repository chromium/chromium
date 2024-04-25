// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_backup_agent.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/ChromeBackupAgentImpl_jni.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_prefs.h"

static_assert(52 == syncer::GetNumModelTypes(),
              "If the new type has a corresponding pref, add it to "
              "ChromeBackupAgentImpl.BACKUP_NATIVE_BOOL_PREFS");

void JNI_ChromeBackupAgentImpl_CommitPendingPrefWrites(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_prefs) {
  // TODO(crbug.com/332710541): This currently doesn't wait for the commit to
  // complete (it passes the default value for the reply_callback param). Wait
  // for the commit to complete, here or in Java.
  PrefServiceAndroid::FromPrefServiceAndroid(j_prefs)->CommitPendingWrite();
}

std::string JNI_ChromeBackupAgentImpl_GetSerializedDict(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_prefs,
    std::string& pref_name) {
  return chrome_backup_agent::GetSerializedDict(
      PrefServiceAndroid::FromPrefServiceAndroid(j_prefs), pref_name);
}

void JNI_ChromeBackupAgentImpl_SetDict(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_prefs,
    std::string& pref_name,
    std::string& serialized_dict) {
  chrome_backup_agent::SetDict(
      PrefServiceAndroid::FromPrefServiceAndroid(j_prefs), pref_name,
      serialized_dict);
}

void JNI_ChromeBackupAgentImpl_MigrateGlobalDataTypePrefsToAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_prefs,
    std::string& gaia_id) {
  PrefService* pref_service =
      PrefServiceAndroid::FromPrefServiceAndroid(j_prefs);
  syncer::SyncPrefs sync_prefs(pref_service);
  sync_prefs.MigrateGlobalDataTypePrefsToAccount(
      pref_service, signin::GaiaIdHash::FromGaiaId(gaia_id));
}

namespace chrome_backup_agent {

std::string GetSerializedDict(PrefService* pref_service,
                              const std::string& pref_name) {
  std::string serialized_dict;
  const bool serializer_result =
      JSONStringValueSerializer(&serialized_dict)
          .Serialize(pref_service->GetDict(pref_name));
  CHECK(serializer_result);
  return serialized_dict;
}

void SetDict(PrefService* pref_service,
             const std::string& pref_name,
             const std::string& serialized_dict) {
  std::unique_ptr<base::Value> dict =
      JSONStringValueDeserializer(serialized_dict)
          .Deserialize(/*error_code=*/nullptr, /*error_message=*/nullptr);
  if (!dict || !dict->is_dict()) {
    // This should only happen if there was a bug when backing up the data, or
    // if data was corrupted. It's not appropriate to crash for the latter, so
    // just no-op.
    return;
  }
  pref_service->SetDict(pref_name, dict->GetDict().Clone());
}

}  // namespace chrome_backup_agent
