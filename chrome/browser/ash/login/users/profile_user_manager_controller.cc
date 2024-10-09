// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"

#include "base/check.h"
#include "base/check_is_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace ash {

ProfileUserManagerController::ProfileUserManagerController(
    ProfileManager* profile_manager,
    user_manager::UserManager* user_manager)
    : user_manager_(user_manager) {
  profile_manager_observation_.Observe(profile_manager);
}

ProfileUserManagerController::~ProfileUserManagerController() = default;

void ProfileUserManagerController::OnProfileCreationStarted(Profile* profile) {
  // Find a User instance from directory path, and annotate the AccountId.
  // Hereafter, we can use AnnotatedAccountId::Get() to find the User.
  if (ash::IsUserBrowserContext(profile)) {
    auto logged_in_users = user_manager_->GetLoggedInUsers();
    auto it = base::ranges::find(
        logged_in_users,
        ash::BrowserContextHelper::GetUserIdHashFromBrowserContext(profile),
        [](const user_manager::User* user) { return user->username_hash(); });
    if (it == logged_in_users.end()) {
      // User may not be found for now on testing.
      // TODO(crbug.com/40225390): fix tests to annotate AccountId properly.
      CHECK_IS_TEST();
    } else {
      const user_manager::User* user = *it;
      auto* session_manager = session_manager::SessionManager::Get();
      if (session_manager) {
        // A |User| instance should always exist for a profile which is not the
        // initial, the sign-in or the lock screen app profile.
        CHECK(session_manager->HasSessionForAccountId(user->GetAccountId()))
            << "Attempting to construct the profile before starting the user "
               "session";
      } else {
        // SessionManager should be always initialized before Profile creation,
        // except tests.
        CHECK_IS_TEST();
      }
      ash::AnnotatedAccountId::Set(profile, user->GetAccountId(),
                                   /*for_test=*/false);
    }
  }
}

void ProfileUserManagerController::OnProfileAdded(Profile* profile) {
  // TODO(crbug.com/40225390): Use ash::AnnotatedAccountId::Get(), when
  // it gets fully ready for tests.
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    return;
  }

  // Guest users should use OTR profiles.
  if (user->GetType() == user_manager::UserType::kGuest) {
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  if (user_manager_->OnUserProfileCreated(user->GetAccountId(),
                                          profile->GetPrefs())) {
    // Add observer for graceful shutdown of User on Profile destruction.
    auto observation =
        std::make_unique<base::ScopedObservation<Profile, ProfileObserver>>(
            this);
    observation->Observe(profile);
    profile_observations_.push_back(std::move(observation));
  }
}

void ProfileUserManagerController::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void ProfileUserManagerController::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK(std::erase_if(profile_observations_, [profile](auto& observation) {
    return observation->IsObservingSource(profile);
  }));
  // TODO(crbug.com/40225390): User ash::AnnotatedAccountId::Get(), when it gets
  // fully ready for tests.
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user) {
    user_manager_->OnUserProfileWillBeDestroyed(user->GetAccountId());
  }
}

}  // namespace ash
