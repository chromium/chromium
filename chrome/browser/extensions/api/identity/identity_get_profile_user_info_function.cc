// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"

#include "base/notreached.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "google_apis/gaia/gaia_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {
CoreAccountInfo GetAccountInfoFromProfileDetails(
    const std::optional<api::identity::ProfileDetails>& details,
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service) {
  const api::identity::AccountStatus account_status =
      details ? details->account_status : api::identity::AccountStatus::kNone;
  const CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account_info.IsEmpty()) {
    return CoreAccountInfo();
  }

  switch (account_status) {
    case api::identity::AccountStatus::kAny:
      return primary_account_info;
    case api::identity::AccountStatus::kNone:
    case api::identity::AccountStatus::kSync:
      return sync_service &&
                     sync_service->GetUserSettings()->GetSelectedTypes().Has(
                         syncer::UserSelectableType::kExtensions)
                 ? primary_account_info
                 : CoreAccountInfo();
  }

  NOTREACHED() << "Unexpected value for account_status: "
               << api::identity::ToString(account_status);
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

  const std::optional<api::identity::GetProfileUserInfo::Params> params =
      api::identity::GetProfileUserInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  api::identity::ProfileUserInfo profile_user_info;

  if (extension()->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kIdentityEmail)) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    const CoreAccountInfo account_info = GetAccountInfoFromProfileDetails(
        params->details, IdentityManagerFactory::GetForProfile(profile),
        SyncServiceFactory::GetForProfile(profile));
    profile_user_info.email = account_info.email;
    profile_user_info.id = account_info.gaia.ToString();
  }

  return RespondNow(WithArguments(profile_user_info.ToValue()));
}

}  // namespace extensions
