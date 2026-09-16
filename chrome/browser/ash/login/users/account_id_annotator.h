// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_ACCOUNT_ID_ANNOTATOR_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_ACCOUNT_ID_ANNOTATOR_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

class Profile;

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

// Annotates AccountId to the Profile on creation in production.
// In tests, ScopedAccountIdAnnotator is used instead to annotate AccountId
// explicitly without relying on directory path naming conventions.
class AccountIdAnnotator : public ProfileManagerObserver {
 public:
  AccountIdAnnotator(ProfileManager* profile_manager,
                     user_manager::UserManager* user_manager);
  AccountIdAnnotator(const AccountIdAnnotator&) = delete;
  AccountIdAnnotator& operator=(const AccountIdAnnotator&) = delete;
  ~AccountIdAnnotator() override;

  // ProfileManagerObserver:
  void OnProfileCreationStarted(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  const raw_ref<user_manager::UserManager> user_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_ACCOUNT_ID_ANNOTATOR_H_
