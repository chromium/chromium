// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_MULTI_PROFILE_USER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_MULTI_PROFILE_USER_CONTROLLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace user_manager {
class User;
class UserManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ash {

// MultiProfileUserController decides whether a user is allowed to be in a
// multi-profiles session. It caches the multi-profile user behavior pref backed
// by user policy into local state so that the value is available before the
// user login and checks if the meaning of the value is respected.
class MultiProfileUserController {
 public:
  MultiProfileUserController(PrefService* local_state,
                             user_manager::UserManager* user_manager);

  MultiProfileUserController(const MultiProfileUserController&) = delete;
  MultiProfileUserController& operator=(const MultiProfileUserController&) =
      delete;

  ~MultiProfileUserController();

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Stops to work.
  void Shutdown();

  // Returns the cached policy value for `user_email`.
  user_manager::MultiUserSignInPolicy GetCachedValue(
      std::string_view user_email) const;

  // Returns the primary user's policy. If there's no primary user,
  // returns std::nullopt.
  std::optional<user_manager::MultiUserSignInPolicy> GetPrimaryUserPolicy()
      const;

  // Returns true if user allowed to be in the current session.
  bool IsUserAllowedInSession(const std::string& user_email) const;

  // Starts to observe the multiprofile user behavior pref of the given user.
  void StartObserving(user_manager::User* user);

  // Removes the cached values for the given user.
  void RemoveCachedValues(std::string_view user_email);

 private:
  friend class MultiProfileUserControllerTest;

  // Sets the cached policy value.
  void SetCachedValue(std::string_view user_email,
                      user_manager::MultiUserSignInPolicy policy);

  // Checks if all users are allowed in the current session.
  void CheckSessionUsers();

  // Invoked when user behavior pref value changes.
  void OnUserPrefChanged(user_manager::User* user);

  raw_ptr<PrefService, DanglingUntriaged> local_state_;
  raw_ptr<user_manager::UserManager> user_manager_;
  std::vector<std::unique_ptr<PrefChangeRegistrar>> pref_watchers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_MULTI_PROFILE_USER_CONTROLLER_H_
