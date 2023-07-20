// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_clear_all_cached_auth_tokens_function.h"

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

IdentityClearAllCachedAuthTokensFunction::
    IdentityClearAllCachedAuthTokensFunction() = default;
IdentityClearAllCachedAuthTokensFunction::
    ~IdentityClearAllCachedAuthTokensFunction() = default;

ExtensionFunction::ResponseAction
IdentityClearAllCachedAuthTokensFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(identity_constants::kOffTheRecord));

  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->Get(profile);
  id_api->EraseGaiaIdForExtension(extension()->id());
  id_api->token_cache()->EraseAllTokensForExtension(extension()->id());

  return RespondNow(NoArguments());
}

}  // namespace extensions
