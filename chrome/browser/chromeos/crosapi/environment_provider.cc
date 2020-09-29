// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/environment_provider.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

crosapi::EnvironmentProvider::EnvironmentProvider() = default;
crosapi::EnvironmentProvider::~EnvironmentProvider() = default;

crosapi::mojom::SessionType crosapi::EnvironmentProvider::GetSessionType() {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (profile->IsGuestSession()) {
    return crosapi::mojom::SessionType::kGuestSession;
  }
  if (profiles::IsPublicSession()) {
    return crosapi::mojom::SessionType::kPublicSession;
  }
  return crosapi::mojom::SessionType::kRegularSession;
}
