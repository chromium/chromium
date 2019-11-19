// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"

#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

IdentityGetProfileUserInfoFunction::IdentityGetProfileUserInfoFunction() {
}

IdentityGetProfileUserInfoFunction::~IdentityGetProfileUserInfoFunction() {
}

ExtensionFunction::ResponseAction IdentityGetProfileUserInfoFunction::Run() {
  if (browser_context()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  api::identity::ProfileUserInfo profile_user_info;

  if (extension()->permissions_data()->HasAPIPermission(
          APIPermission::kIdentityEmail)) {
    auto account_info = IdentityManagerFactory::GetForProfile(
                            Profile::FromBrowserContext(browser_context()))
                            ->GetPrimaryAccountInfo();
    profile_user_info.email = account_info.email;
    profile_user_info.id = account_info.gaia;
  }

  return RespondNow(OneArgument(profile_user_info.ToValue()));
}

}  // namespace extensions
