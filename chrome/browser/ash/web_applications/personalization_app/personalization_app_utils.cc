// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {
namespace personalization_app {

const user_manager::User* GetUser(const Profile* profile) {
  auto* profile_helper = chromeos::ProfileHelper::Get();
  DCHECK(profile_helper);
  const user_manager::User* user = profile_helper->GetUserByProfile(profile);
  DCHECK(user);
  return user;
}

AccountId GetAccountId(const Profile* profile) {
  return GetUser(profile)->GetAccountId();
}

}  // namespace personalization_app
}  // namespace ash
