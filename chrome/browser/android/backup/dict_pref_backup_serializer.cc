// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/backup/dict_pref_backup_serializer.h"

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DictPrefBackupSerializer_jni.h"

std::string JNI_DictPrefBackupSerializer_GetSerializedDict(
    JNIEnv* env,
    PrefService* pref_service,
    std::string& pref_name) {
  return dict_pref_backup_serializer::GetSerializedDict(pref_service,
                                                        pref_name);
}

void JNI_DictPrefBackupSerializer_SetDict(JNIEnv* env,
                                          PrefService* pref_service,
                                          std::string& pref_name,
                                          std::string& serialized_dict) {
  dict_pref_backup_serializer::SetDict(pref_service, pref_name,
                                       serialized_dict);
}

namespace dict_pref_backup_serializer {

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

}  // namespace dict_pref_backup_serializer
