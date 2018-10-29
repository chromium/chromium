// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/supports_user_data.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace signin_util {
namespace {

constexpr char kSignoutSettingKey[] = "signout_setting";

class UserSignoutSetting : public base::SupportsUserData::Data {
 public:
  // Fetch from Profile. Make and store if not already present.
  static UserSignoutSetting* GetForProfile(Profile* profile) {
    UserSignoutSetting* signout_setting = static_cast<UserSignoutSetting*>(
        profile->GetUserData(kSignoutSettingKey));

    if (!signout_setting) {
      profile->SetUserData(kSignoutSettingKey,
                           std::make_unique<UserSignoutSetting>());
      signout_setting = static_cast<UserSignoutSetting*>(
          profile->GetUserData(kSignoutSettingKey));
    }

    return signout_setting;
  }

  bool is_user_signout_allowed() const { return is_user_signout_allowed_; }
  void set_is_user_signout_allowed(bool is_allowed) {
    is_user_signout_allowed_ = is_allowed;
  }

 private:
  // User sign-out allowed by default.
  bool is_user_signout_allowed_ = true;
};

enum ForceSigninPolicyCache {
  NOT_CACHED = 0,
  ENABLE,
  DISABLE
} g_is_force_signin_enabled_cache = NOT_CACHED;

void SetForceSigninPolicy(bool enable) {
  g_is_force_signin_enabled_cache = enable ? ENABLE : DISABLE;
}

}  // namespace

bool IsForceSigninEnabled() {
  if (g_is_force_signin_enabled_cache == NOT_CACHED) {
    PrefService* prefs = g_browser_process->local_state();
    if (prefs)
      SetForceSigninPolicy(prefs->GetBoolean(prefs::kForceBrowserSignin));
    else
      return false;
  }
  return (g_is_force_signin_enabled_cache == ENABLE);
}

void SetForceSigninForTesting(bool enable) {
  SetForceSigninPolicy(enable);
}

void ResetForceSigninForTesting() {
  g_is_force_signin_enabled_cache = NOT_CACHED;
}

void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed) {
  UserSignoutSetting::GetForProfile(profile)->set_is_user_signout_allowed(
      is_allowed);
}

bool IsUserSignoutAllowedForProfile(Profile* profile) {
  return UserSignoutSetting::GetForProfile(profile)->is_user_signout_allowed();
}

}  // namespace signin_util
