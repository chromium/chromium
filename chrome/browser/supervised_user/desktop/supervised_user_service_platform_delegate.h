// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_DESKTOP_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_SUPERVISED_USER_DESKTOP_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_

#include "chrome/browser/supervised_user/chrome_supervised_user_service_platform_delegate_base.h"

class Profile;

class SupervisedUserServicePlatformDelegate
    : public ChromeSupervisedUserServicePlatformDelegateBase {
 public:
  explicit SupervisedUserServicePlatformDelegate(Profile& profile);

  // SupervisedUserService::PlatformDelegate
  void CloseIncognitoTabs() override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_DESKTOP_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
