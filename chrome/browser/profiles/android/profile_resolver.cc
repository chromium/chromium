// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/android/profile_resolver.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/android/proto/profile_token.pb.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_key_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/profiles/android/jni_headers/ProfileResolver_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace profile_resolver {

namespace {

base::FilePath GetUserDataDir() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir;
}

base::FilePath GetAbsoluteProfilePath(const std::string& relative_string) {
  return GetUserDataDir().Append(relative_string);
}

base::FilePath GetRelativeProfilePath(const base::FilePath& profile_path) {
  base::FilePath user_data_dir = GetUserDataDir();
  base::FilePath relative_path;
  bool success =
      GetUserDataDir().AppendRelativePath(profile_path, &relative_path);
  DCHECK(success);
  return relative_path;
}

void OnProfileLoadedFromManager(ProfileToken token_proto,
                                ProfileCallback callback,
                                Profile* profile) {
  if (profile && !token_proto.otr_profile_id().empty()) {
    Profile::OTRProfileID otrProfileId =
        Profile::OTRProfileID::Deserialize(token_proto.otr_profile_id());
    profile = profile->GetOffTheRecordProfile(otrProfileId,
                                              /*create_if_needed=*/false);
  }

  std::move(callback).Run(profile);
}

void LookupProfileByToken(ProfileToken token_proto, ProfileCallback callback) {
  base::FilePath absolute_path =
      GetAbsoluteProfilePath(token_proto.relative_path());
  g_browser_process->profile_manager()->LoadProfileByPath(
      absolute_path,
      /*incognito=*/false,
      base::BindOnce(&OnProfileLoadedFromManager, std::move(token_proto),
                     std::move(callback)));
}

void ProfileToProfileKey(ProfileKeyCallback callback, Profile* profile) {
  std::move(callback).Run(profile ? profile->GetProfileKey() : nullptr);
}

void OnResolvedProfile(const JavaRef<jobject>& j_callback, Profile* profile) {
  ScopedJavaLocalRef<jobject> j_profile;
  if (profile) {
    j_profile = profile->GetJavaObject();
  }
  base::android::RunObjectCallbackAndroid(j_callback, j_profile);
}

void OnResolvedProfileKey(const JavaRef<jobject>& j_callback,
                          ProfileKey* profile_key) {
  ScopedJavaLocalRef<jobject> j_profile_key;
  if (profile_key) {
    ProfileKeyAndroid* profile_key_android =
        profile_key->GetProfileKeyAndroid();
    j_profile_key = profile_key_android->GetJavaObject();
  }
  base::android::RunObjectCallbackAndroid(j_callback, j_profile_key);
}

}  // namespace

void ResolveProfile(std::string token, ProfileCallback callback) {
  ProfileToken token_proto;
  token_proto.ParseFromString(token);
  return LookupProfileByToken(std::move(token_proto), std::move(callback));
}

void ResolveProfileKey(std::string token, ProfileKeyCallback callback) {
  ProfileToken token_proto;
  token_proto.ParseFromString(token);

  // This will be null if the profile infra has started up. It will be non null
  // when we're in reduced mode and can only use ProfileKey/SimpleFactoryKey.
  ProfileKey* startup_profile_key =
      ProfileKeyStartupAccessor::GetInstance()->profile_key();

  if (startup_profile_key) {
    // TODO(crbug.com/40753680): Does not currently support OTR
    // resolution without profile infra.
    if (!token_proto.otr_profile_id().empty()) {
      std::move(callback).Run(nullptr);
      return;
    }

    // In reduced mode only the main profile's key can be returned. Other
    // profiles will resolve to null.
    std::string relative_path =
        GetRelativeProfilePath(startup_profile_key->GetPath()).value();
    if (relative_path != token_proto.relative_path()) {
      std::move(callback).Run(nullptr);
      return;
    }

    std::move(callback).Run(startup_profile_key);
  } else {
    // Profile infra has already initialized, get it from the Profile instead
    // because this supports more general resolution.
    LookupProfileByToken(
        std::move(token_proto),
        base::BindOnce(&ProfileToProfileKey, std::move(callback)));
  }
}

std::string TokenizeProfile(Profile* profile) {
  if (!profile) {
    return std::string();
  }

  ProfileToken token_proto;
  token_proto.set_relative_path(
      GetRelativeProfilePath(profile->GetPath()).value());
  if (profile->IsOffTheRecord()) {
    token_proto.set_otr_profile_id(profile->GetOTRProfileID().Serialize());
  }

  std::string token_string;
  token_proto.SerializeToString(&token_string);
  return token_string;
}

std::string TokenizeProfileKey(ProfileKey* profile_key) {
  if (!profile_key) {
    return std::string();
  }

  // TODO(crbug.com/40753680): Does not currently support tokenization of
  // OTR ProfileKeys. They don't hold a OTRProfileID value.
  DCHECK(!profile_key->IsOffTheRecord());

  ProfileToken token_proto;
  token_proto.set_relative_path(
      GetRelativeProfilePath(profile_key->GetPath()).value());

  std::string token_string;
  token_proto.SerializeToString(&token_string);
  return token_string;
}

static void JNI_ProfileResolver_ResolveProfile(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_token,
    const JavaParamRef<jobject>& j_callback) {
  if (!j_token.obj()) {
    base::android::RunObjectCallbackAndroid(j_callback,
                                            ScopedJavaLocalRef<jobject>());
    return;
  }

  std::string token = ConvertJavaStringToUTF8(env, j_token);
  ResolveProfile(
      std::move(token),
      base::BindOnce(&OnResolvedProfile,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

static void JNI_ProfileResolver_ResolveProfileKey(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_token,
    const JavaParamRef<jobject>& j_callback) {
  if (!j_token.obj()) {
    base::android::RunObjectCallbackAndroid(j_callback,
                                            ScopedJavaLocalRef<jobject>());
    return;
  }

  std::string token = ConvertJavaStringToUTF8(env, j_token);
  ResolveProfileKey(
      std::move(token),
      base::BindOnce(&OnResolvedProfileKey,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

static ScopedJavaLocalRef<jstring> JNI_ProfileResolver_TokenizeProfile(
    JNIEnv* env,
    Profile* profile) {
  return ConvertUTF8ToJavaString(env, TokenizeProfile(profile));
}

static ScopedJavaLocalRef<jstring> JNI_ProfileResolver_TokenizeProfileKey(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_key) {
  ProfileKey* profile_key =
      ProfileKeyAndroid::FromProfileKeyAndroid(j_profile_key);
  return ConvertUTF8ToJavaString(env, TokenizeProfileKey(profile_key));
}

}  // namespace profile_resolver
