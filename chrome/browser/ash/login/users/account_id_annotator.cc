// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/account_id_annotator.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

AccountIdAnnotator::AccountIdAnnotator(ProfileManager* profile_manager,
                                       user_manager::UserManager* user_manager)
    : user_manager_(CHECK_DEREF(user_manager)) {
  profile_manager_observation_.Observe(profile_manager);
}

AccountIdAnnotator::~AccountIdAnnotator() = default;

void AccountIdAnnotator::OnProfileCreationStarted(Profile* profile) {
  // Find a User instance from directory path, and annotate the AccountId.
  // Hereafter, we can use AnnotatedAccountId::Get() to find the User.
  if (ash::IsUserBrowserContext(profile)) {
    auto logged_in_users = user_manager_->GetLoggedInUsers();
    auto it = std::ranges::find(
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

void AccountIdAnnotator::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

}  // namespace ash
