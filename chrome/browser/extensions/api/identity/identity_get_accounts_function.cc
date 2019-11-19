// Copyright 2017 The Chromium Authors. All rights reserved.
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

  std::vector<CoreAccountInfo> accounts =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()))
          ->GetAccountsWithRefreshTokens();
  std::unique_ptr<base::ListValue> infos(new base::ListValue());

  if (accounts.empty()) {
    return RespondNow(OneArgument(std::move(infos)));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  bool primary_account_only = IdentityAPI::GetFactoryInstance()
                                  ->Get(profile)
                                  ->AreExtensionsRestrictedToPrimaryAccount();

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  api::identity::AccountInfo account_info;

  // Ensure that the primary account is inserted first; even though this
  // semantics isn't documented, the implementation has always ensured it and it
  // shouldn't be changed without determining that it is safe to do so.
  if (identity_manager->HasPrimaryAccountWithRefreshToken()) {
    account_info.id = identity_manager->GetPrimaryAccountInfo().gaia;
    infos->Append(account_info.ToValue());
  }

  // If secondary accounts are supported, add all the secondary accounts as
  // well.
  if (!primary_account_only) {
    for (const auto& account : accounts) {
      if (account.account_id == identity_manager->GetPrimaryAccountId())
        continue;
      account_info.id = account.gaia;
      infos->Append(account_info.ToValue());
    }
  }

  return RespondNow(OneArgument(std::move(infos)));
}

}  // namespace extensions
