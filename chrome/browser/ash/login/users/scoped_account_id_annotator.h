// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_SCOPED_ACCOUNT_ID_ANNOTATOR_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_SCOPED_ACCOUNT_ID_ANNOTATOR_H_

#include <optional>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/account_id/account_id.h"

class Profile;
class ProfileManager;

namespace ash {

// In production, ProfileUserManagerController annotates AccountId via
// ProfileManagerObserver::OnProfileCreationStarted. However, the calculation
// of the AccountId is inferred from the directory path, which is slightly
// inconvenient for writing tests. This registers the same observer, and
// annotates the `account_id` passed to the constructor to the Profile
// created in the given `profile_manager`.
class ScopedAccountIdAnnotator : public ProfileManagerObserver {
 public:
  ScopedAccountIdAnnotator(ProfileManager* profile_manager,
                           const AccountId& account_id);
  ~ScopedAccountIdAnnotator() override;

  // ProfileManagerObserver:
  void OnProfileCreationStarted(Profile* profile) override;

 private:
  std::optional<AccountId> account_id_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver> observation_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_SCOPED_ACCOUNT_ID_ANNOTATOR_H_
