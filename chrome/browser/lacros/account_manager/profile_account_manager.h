// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

class ProfileAccountManager : public KeyedService {
 public:
  ProfileAccountManager();
  ~ProfileAccountManager() override;

  ProfileAccountManager(const ProfileAccountManager&) = delete;
  ProfileAccountManager& operator=(const ProfileAccountManager&) = delete;
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_PROFILE_ACCOUNT_MANAGER_H_
