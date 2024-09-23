// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/desktop/supervised_user_service_platform_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"

SupervisedUserServicePlatformDelegate::SupervisedUserServicePlatformDelegate(
    Profile& profile)
    : ChromeSupervisedUserServicePlatformDelegateBase(profile) {}

void SupervisedUserServicePlatformDelegate::CloseIncognitoTabs() {
  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/* create_if_needed =*/false);
  if (otr_profile) {
    BrowserList::CloseAllBrowsersWithIncognitoProfile(
        otr_profile, base::DoNothing(), base::DoNothing(),
        /*skip_beforeunload=*/true);
  }
}
