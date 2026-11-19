// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/android/profile_resolver.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "chrome/browser/android/proto/profile_token.pb.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/profiles/android/jni_headers/ProfileResolver_jni.h"

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
  CHECK(success, base::NotFatalUntil::M162);
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

}  // namespace

void ResolveProfile(std::string token, ProfileCallback callback) {
  ProfileToken token_proto;
  token_proto.ParseFromString(token);
  return LookupProfileByToken(std::move(token_proto), std::move(callback));
}

// The returned token is persisted (e.g. in PendingIntents); see the comment on
// ProfileToken before changing what is serialized here.
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

// TODO(crbug.com/40254448): Remove once BackgroundTabPoolManager uses the
// encoded token from ProfileTokenUtils and legacy intents have expired
// (crbug.com/585922618).
static void JNI_ProfileResolver_ResolveProfile(
    std::optional<std::string> token,
    base::OnceCallback<void(Profile*)> callback) {
  if (!token) {
    std::move(callback).Run(nullptr);
    return;
  }

  ResolveProfile(std::move(*token), std::move(callback));
}

static std::string JNI_ProfileResolver_TokenizeProfile(Profile* profile) {
  return TokenizeProfile(profile);
}

// Like JNI_ProfileResolver_TokenizeProfile(), but passes the token's bytes to
// Java unchanged rather than converting them to a Java String.
static std::vector<uint8_t> JNI_ProfileResolver_TokenizeProfileToBytes(
    Profile* profile) {
  std::string token = TokenizeProfile(profile);
  return std::vector<uint8_t>(token.begin(), token.end());
}

static void JNI_ProfileResolver_ResolveProfileFromBytes(
    std::vector<uint8_t> token,
    base::OnceCallback<void(Profile*)> callback) {
  ResolveProfile(std::string(token.begin(), token.end()), std::move(callback));
}

}  // namespace profile_resolver

DEFINE_JNI(ProfileResolver)
