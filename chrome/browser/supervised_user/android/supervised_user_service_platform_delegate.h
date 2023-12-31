// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_ANDROID_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_SUPERVISED_USER_ANDROID_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_

#include "components/supervised_user/core/browser/supervised_user_service.h"

class SupervisedUserServicePlatformDelegate
    : public supervised_user::SupervisedUserService::PlatformDelegate {
 public:
  SupervisedUserServicePlatformDelegate();

  // SupervisedUserService::PlatformDelegate
  void CloseIncognitoTabs() override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_ANDROID_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
