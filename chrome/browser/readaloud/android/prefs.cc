// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/android/prefs.h"

#include "base/android/jni_string.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/readaloud/android/jni_headers/ReadAloudPrefs_jni.h"

using jni_zero::JavaRef;

namespace readaloud {
namespace {
inline constexpr char kReadAloudSalt[] = "readaloud.salt";
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kReadAloudVoiceSettings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kReadAloudSpeed, 1.,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kReadAloudHighlightingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kListenToThisPageEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterUint64Pref(kReadAloudSalt, 0);
  registry->RegisterIntegerPref(prefs::kReadAloudPlaybackMode, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kReadAloudSyntheticTrials);
}

static void JNI_ReadAloudPrefs_GetVoices(JNIEnv* env,
                                         PrefService* prefs,
                                         const JavaRef<jobject>& j_output_map) {
  const base::DictValue& dict = prefs->GetDict(prefs::kReadAloudVoiceSettings);
  for (auto [language, value] : dict) {
    jni_zero::MapPut(env, j_output_map, language, value.GetString());
  }
}

static void JNI_ReadAloudPrefs_SetVoice(PrefService* prefs,
                                        const std::string& language,
                                        const std::string& voice_id) {
  ScopedDictPrefUpdate(prefs, prefs::kReadAloudVoiceSettings)
      ->Set(language, voice_id);
}

static int64_t JNI_ReadAloudPrefs_GetReliabilityLoggingId(
    PrefService* prefs,
    const std::string& metrics_id) {
  if (!prefs) {
    return 0L;
  }
  return GetReliabilityLoggingId(*prefs, metrics_id);
}

uint64_t GetReliabilityLoggingId(PrefService& prefs,
                                 const std::string& metrics_id) {
  if (metrics_id.empty()) {
    return 0L;
  }

  uint64_t salt;
  if (!prefs.HasPrefPath(kReadAloudSalt)) {
    salt = base::RandUint64();
    prefs.SetUint64(kReadAloudSalt, salt);
  } else {
    salt = prefs.GetUint64(kReadAloudSalt);
  }
  return base::FastHash(base::StrCat(
      {metrics_id, std::string(reinterpret_cast<char*>(&salt), sizeof(salt))}));
}

}  // namespace readaloud

DEFINE_JNI(ReadAloudPrefs)
