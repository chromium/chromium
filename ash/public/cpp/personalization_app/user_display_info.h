// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PERSONALIZATION_APP_USER_DISPLAY_INFO_H_
#define ASH_PUBLIC_CPP_PERSONALIZATION_APP_USER_DISPLAY_INFO_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "components/user_manager/user.h"
#include "url/gurl.h"

namespace ash::personalization_app {

struct ASH_PUBLIC_EXPORT UserDisplayInfo {
  // The display email of the user.
  std::string email;

  // The display name of the user.
  std::string name;

  UserDisplayInfo();
  explicit UserDisplayInfo(const user_manager::User& user_info);

  UserDisplayInfo(const UserDisplayInfo& other) = delete;
  UserDisplayInfo& operator=(const UserDisplayInfo&) = delete;

  UserDisplayInfo(UserDisplayInfo&& other);
  UserDisplayInfo& operator=(UserDisplayInfo&&);

  ~UserDisplayInfo();
};

}  // namespace ash::personalization_app

#endif  // ASH_PUBLIC_CPP_PERSONALIZATION_APP_USER_DISPLAY_INFO_H_
