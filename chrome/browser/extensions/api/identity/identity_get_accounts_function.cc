// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

IdentityGetAccountsFunction::IdentityGetAccountsFunction() {
}

IdentityGetAccountsFunction::~IdentityGetAccountsFunction() {
}

ExtensionFunction::ResponseAction IdentityGetAccountsFunction::Run() {
  if (browser_context()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  IdentityAPI* identity_api = IdentityAPI::GetFactoryInstance()->Get(profile);
  std::vector<CoreAccountInfo> accounts =
      identity_api->GetAccountsWithRefreshTokensForExtensions();
  base::Value::List infos;

  if (accounts.empty()) {
    return RespondNow(WithArguments(std::move(infos)));
  }

  bool primary_account_only =
      identity_api->AreExtensionsRestrictedToPrimaryAccount();

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  api::identity::AccountInfo account_info;

  // Ensure that the primary account is inserted first; even though this
  // semantics isn't documented, the implementation has always ensured it and it
  // shouldn't be changed without determining that it is safe to do so.
  if (identity_manager->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSync)) {
    account_info.id =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
            .gaia;
    infos.Append(base::Value(account_info.ToValue()));
  }

  // If secondary accounts are supported, add all the secondary accounts as
  // well.
  if (!primary_account_only) {
    for (const auto& account : accounts) {
      if (account.account_id ==
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync))
        continue;
      account_info.id = account.gaia;
      infos.Append(base::Value(account_info.ToValue()));
    }
  }

  return RespondNow(WithArguments(std::move(infos)));
}

}  // namespace extensions
