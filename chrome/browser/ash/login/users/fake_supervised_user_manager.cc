// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/fake_supervised_user_manager.h"

#include <string>

namespace ash {

FakeSupervisedUserManager::FakeSupervisedUserManager() {}

FakeSupervisedUserManager::~FakeSupervisedUserManager() {}

std::string FakeSupervisedUserManager::GetUserSyncId(
    const std::string& supervised_user_id) const {
  return std::string();
}

std::u16string FakeSupervisedUserManager::GetManagerDisplayName(
    const std::string& supervised_user_id) const {
  return std::u16string();
}

std::string FakeSupervisedUserManager::GetManagerUserId(
    const std::string& supervised_user_id) const {
  return std::string();
}

std::string FakeSupervisedUserManager::GetManagerDisplayEmail(
    const std::string& supervised_user_id) const {
  return std::string();
}

}  // namespace ash
