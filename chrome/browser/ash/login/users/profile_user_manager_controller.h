// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_PROFILE_USER_MANAGER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_PROFILE_USER_MANAGER_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

class Profile;

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

// Observes Profile related events, and triggers related methods defined
// in UserManager.
// To minimize the dependency between OS system and Chrome as a web browser
// in the code base, this class must be kept as small as possible.
class ProfileUserManagerController : public ProfileManagerObserver,
                                     public ProfileObserver {
 public:
  ProfileUserManagerController(ProfileManager* profile_manager,
                               user_manager::UserManager* user_manager);
  ProfileUserManagerController(const ProfileUserManagerController&) = delete;
  ProfileUserManagerController& operator=(const ProfileUserManagerController&) =
      delete;
  ~ProfileUserManagerController() override;

  // ProfileManagerObserver:
  void OnProfileCreationStarted(Profile* profile) override;
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  std::vector<
      std::unique_ptr<base::ScopedObservation<Profile, ProfileObserver>>>
      profile_observations_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  raw_ptr<user_manager::UserManager> user_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_PROFILE_USER_MANAGER_CONTROLLER_H_
