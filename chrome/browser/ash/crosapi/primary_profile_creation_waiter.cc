// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/primary_profile_creation_waiter.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

PrimaryProfileCreationWaiter::PrimaryProfileCreationWaiter(
    ProfileManager* profile_manager,
    base::OnceClosure callback)
    : profile_manager_(profile_manager), callback_(std::move(callback)) {
  profile_manager_observation_.Observe(profile_manager_.get());
}

PrimaryProfileCreationWaiter::~PrimaryProfileCreationWaiter() = default;

std::unique_ptr<PrimaryProfileCreationWaiter>
PrimaryProfileCreationWaiter::WaitOrRun(ProfileManager* profile_manager,
                                        base::OnceClosure callback) {
  // If the primary user's profile has already been created, run callback now.
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user && primary_user->is_profile_created()) {
    std::move(callback).Run();
    return nullptr;
  }
  // Otherwise, return a waiter object which will invoke the callback
  // once the profile for the primary user has been created.
  return base::WrapUnique(
      new PrimaryProfileCreationWaiter(profile_manager, std::move(callback)));
}

void PrimaryProfileCreationWaiter::OnProfileAdded(Profile* profile) {
  // If the profile is the primary user's profile, run the callback.
  auto* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (user_manager::UserManager::Get()->IsPrimaryUser(user)) {
    profile_manager_observation_.Reset();
    std::move(callback_).Run();
  }
}

}  // namespace crosapi
