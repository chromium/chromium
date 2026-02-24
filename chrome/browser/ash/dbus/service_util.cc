// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/service_util.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

Profile* GetProfileFromUserIdHash(const std::string& user_id_hash) {
  if (user_id_hash.empty()) {
    return ProfileManager::GetActiveUserProfile();
  }

  user_manager::User* user = nullptr;
  for (user_manager::User* logged_in_user :
       user_manager::UserManager::Get()->GetLoggedInUsers()) {
    if (logged_in_user->username_hash() == user_id_hash) {
      user = logged_in_user;
      break;
    }
  }
  if (!user) {
    return nullptr;
  }
  return Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(user));
}

}  // namespace ash
