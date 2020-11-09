// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/fake_supervised_user_manager.h"

#include <string>

namespace chromeos {

FakeSupervisedUserManager::FakeSupervisedUserManager() {}

FakeSupervisedUserManager::~FakeSupervisedUserManager() {}

std::string FakeSupervisedUserManager::GetUserSyncId(
    const std::string& supervised_user_id) const {
  return std::string();
}

base::string16 FakeSupervisedUserManager::GetManagerDisplayName(
    const std::string& supervised_user_id) const {
  return base::string16();
}

std::string FakeSupervisedUserManager::GetManagerUserId(
    const std::string& supervised_user_id) const {
  return std::string();
}

std::string FakeSupervisedUserManager::GetManagerDisplayEmail(
    const std::string& supervised_user_id) const {
  return std::string();
}

}  // namespace chromeos
