// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/service_manager_connection.h"
#include "services/identity/public/mojom/account.mojom.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {

IdentityGetAccountsFunction::IdentityGetAccountsFunction() {
}

IdentityGetAccountsFunction::~IdentityGetAccountsFunction() {
}

ExtensionFunction::ResponseAction IdentityGetAccountsFunction::Run() {
  if (browser_context()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  content::BrowserContext::GetConnectorFor(browser_context())
      ->BindInterface(identity::mojom::kServiceName,
                      mojo::MakeRequest(&identity_manager_));

  identity_manager_->GetAccounts(
      base::BindOnce(&IdentityGetAccountsFunction::OnGotAccounts, this));

  return RespondLater();
}

void IdentityGetAccountsFunction::OnGotAccounts(
    std::vector<identity::mojom::AccountPtr> accounts) {
  std::unique_ptr<base::ListValue> infos(new base::ListValue());

  if (accounts.empty()) {
    Respond(OneArgument(std::move(infos)));
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  bool primary_account_only = IdentityAPI::GetFactoryInstance()
                                  ->Get(profile)
                                  ->AreExtensionsRestrictedToPrimaryAccount();

  // If extensions are restricted to the primary account and there is no valid
  // primary account, short-circuit out.
  if (primary_account_only && (!accounts[0]->state.is_primary_account ||
                               !accounts[0]->state.has_refresh_token)) {
    Respond(OneArgument(std::move(infos)));
    return;
  }

  for (const auto& account : accounts) {
    api::identity::AccountInfo account_info;
    account_info.id = account->info.gaia;
    infos->Append(account_info.ToValue());

    // Stop after the primary account if extensions are restricted to the
    // primary account.
    if (primary_account_only)
      break;
  }

  Respond(OneArgument(std::move(infos)));
}

}  // namespace extensions
