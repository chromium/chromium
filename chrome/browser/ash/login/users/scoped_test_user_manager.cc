// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"

#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"

namespace ash {

ScopedTestUserManager::ScopedTestUserManager() {
  chrome_user_manager_ = ChromeUserManagerImpl::CreateChromeUserManager();
  chrome_user_manager_->Initialize();

  // ProfileHelper has to be initialized after UserManager instance is created.
  ProfileHelper::Get()->Initialize();
}

ScopedTestUserManager::~ScopedTestUserManager() {
  user_manager::UserManager::Get()->Shutdown();
  chrome_user_manager_->Destroy();
  chrome_user_manager_.reset();
}

}  // namespace ash
