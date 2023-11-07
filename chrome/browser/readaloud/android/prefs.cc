// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/android/prefs.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/values.h"
#include "chrome/browser/readaloud/android/jni_headers/ReadAloudPrefs_jni.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::JavaParamRef;
using base::android::MethodID;
using base::android::ScopedJavaLocalRef;

namespace readaloud {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kReadAloudVoiceSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kReadAloudSpeed, 1.,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kReadAloudHighlightingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void JNI_ReadAloudPrefs_GetVoices(JNIEnv* env,
                                  const JavaParamRef<jobject>& j_pref_service,
                                  const JavaParamRef<jobject>& j_output_map) {
  PrefService* prefs =
      PrefServiceAndroid::FromPrefServiceAndroid(j_pref_service);

  ScopedJavaLocalRef<jclass> output_map_class =
      GetClass(env, "java/util/HashMap");
  // jmethodID is a pointer to an internal struct. We don't own it and should
  // not delete it.
  jmethodID map_put_id = MethodID::Get<MethodID::Type::TYPE_INSTANCE>(
      env, output_map_class.obj(), "put",
      "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

  const base::Value::Dict& dict =
      prefs->GetDict(prefs::kReadAloudVoiceSettings);
  for (auto [language, value] : dict) {
    env->CallObjectMethod(
        j_output_map, map_put_id, ConvertUTF8ToJavaString(env, language).obj(),
        ConvertUTF8ToJavaString(env, value.GetString()).obj());
  }
}

void JNI_ReadAloudPrefs_SetVoice(JNIEnv* env,
                                 const JavaParamRef<jobject>& j_pref_service,
                                 const JavaParamRef<jstring>& j_language,
                                 const JavaParamRef<jstring>& j_voice_id) {
  ScopedDictPrefUpdate(
      PrefServiceAndroid::FromPrefServiceAndroid(j_pref_service),
      prefs::kReadAloudVoiceSettings)
      ->Set(ConvertJavaStringToUTF8(env, j_language),
            ConvertJavaStringToUTF8(env, j_voice_id));
}

}  // namespace readaloud
