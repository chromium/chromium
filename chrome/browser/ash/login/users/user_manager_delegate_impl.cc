// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"

#include "chrome/browser/browser_process.h"

namespace ash {

UserManagerDelegateImpl::UserManagerDelegateImpl() = default;
UserManagerDelegateImpl::~UserManagerDelegateImpl() = default;

const std::string& UserManagerDelegateImpl::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace ash
