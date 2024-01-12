// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PRIMARY_PROFILE_CREATION_WAITER_H_
#define CHROME_BROWSER_ASH_CROSAPI_PRIMARY_PROFILE_CREATION_WAITER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

class ProfileManager;

namespace crosapi {

class PrimaryProfileCreationWaiter : public ProfileManagerObserver {
 public:
  ~PrimaryProfileCreationWaiter() override;

  // Disallow copy/assign.
  PrimaryProfileCreationWaiter(const PrimaryProfileCreationWaiter&) = delete;
  PrimaryProfileCreationWaiter& operator=(const PrimaryProfileCreationWaiter&) =
      delete;

  // If the primary profile hasn't been created yet, returns a
  // PrimaryProfileCreationWaiter object which will invoke the
  // callback once the primary profile is ready.
  // If the primary profile has already been created, invokes
  // the callback immediately, and returns nullptr.
  static std::unique_ptr<PrimaryProfileCreationWaiter> WaitOrRun(
      ProfileManager* profile_manager,
      base::OnceClosure callback);

  // ProfileManagerObserver overrides.
  // Called when a profile has been fully created.
  void OnProfileAdded(Profile* profile) override;

 private:
  PrimaryProfileCreationWaiter(ProfileManager* profile_manager,
                               base::OnceClosure callback);

  raw_ptr<ProfileManager> profile_manager_;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  base::OnceClosure callback_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PRIMARY_PROFILE_CREATION_WAITER_H_
