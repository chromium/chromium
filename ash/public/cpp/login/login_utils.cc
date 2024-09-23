// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/public/cpp/login/login_utils.h"

#include <string_view>

#include "ash/public/cpp/session/user_info.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"

namespace ash {

UserAvatar BuildAshUserAvatarForUser(const user_manager::User& user) {
  UserAvatar avatar;
  avatar.image = user.GetImage();
  if (avatar.image.isNull()) {
    avatar.image = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_LOGIN_DEFAULT_USER);
  }

  // TODO(jdufault): Unify image handling between this code and
  // user_image_source::GetUserImageInternal.
  auto load_image_from_resource = [&avatar](int resource_id) {
    auto& rb = ui::ResourceBundle::GetSharedInstance();
    std::string_view avatar_data = rb.GetRawDataResourceForScale(
        resource_id, rb.GetMaxResourceScaleFactor());
    avatar.bytes.assign(avatar_data.begin(), avatar_data.end());
  };

  if (user.has_image_bytes()) {
    avatar.bytes.assign(
        user.image_bytes()->front(),
        user.image_bytes()->front() + user.image_bytes()->size());
  } else if (user.image_is_stub()) {
    load_image_from_resource(IDR_LOGIN_DEFAULT_USER);
  }

  return avatar;
}

UserAvatar BuildAshUserAvatarForAccountId(const AccountId& account_id) {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  CHECK(user_manager);
  const user_manager::User* user = user_manager->FindUser(account_id);
  CHECK(user);
  return BuildAshUserAvatarForUser(*user);
}

}  // namespace ash
