// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"

#include "base/notreached.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {
signin::ConsentLevel GetConsentLevelFromProfileDetails(
    const std::optional<api::identity::ProfileDetails>& details) {
  api::identity::AccountStatus account_status =
      details ? details->account_status : api::identity::AccountStatus::kNone;

  switch (account_status) {
    case api::identity::AccountStatus::kAny:
      return signin::ConsentLevel::kSignin;
    case api::identity::AccountStatus::kNone:
    case api::identity::AccountStatus::kSync:
      return signin::ConsentLevel::kSync;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected value for account_status: "
                            << api::identity::ToString(account_status);
  return signin::ConsentLevel::kSync;
}
}  // namespace

IdentityGetProfileUserInfoFunction::IdentityGetProfileUserInfoFunction() =
    default;

IdentityGetProfileUserInfoFunction::~IdentityGetProfileUserInfoFunction() =
    default;

ExtensionFunction::ResponseAction IdentityGetProfileUserInfoFunction::Run() {
  if (browser_context()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  std::optional<api::identity::GetProfileUserInfo::Params> params =
      api::identity::GetProfileUserInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  api::identity::ProfileUserInfo profile_user_info;

  if (extension()->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kIdentityEmail)) {
    signin::ConsentLevel consent_level =
        GetConsentLevelFromProfileDetails(params->details);
    auto account_info = IdentityManagerFactory::GetForProfile(
                            Profile::FromBrowserContext(browser_context()))
                            ->GetPrimaryAccountInfo(consent_level);
    profile_user_info.email = account_info.email;
    profile_user_info.id = account_info.gaia;
  }

  return RespondNow(WithArguments(profile_user_info.ToValue()));
}

}  // namespace extensions
