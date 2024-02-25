// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_remove_cached_auth_token_function.h"

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"

namespace extensions {

IdentityRemoveCachedAuthTokenFunction::IdentityRemoveCachedAuthTokenFunction() {
}

IdentityRemoveCachedAuthTokenFunction::
    ~IdentityRemoveCachedAuthTokenFunction() {}

ExtensionFunction::ResponseAction IdentityRemoveCachedAuthTokenFunction::Run() {
  if (Profile::FromBrowserContext(browser_context())->IsOffTheRecord())
    return RespondNow(Error(identity_constants::kOffTheRecord));

  std::optional<api::identity::RemoveCachedAuthToken::Params> params =
      api::identity::RemoveCachedAuthToken::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  IdentityAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->token_cache()
      ->EraseAccessToken(extension()->id(), params->details.token);
  return RespondNow(NoArguments());
}

}  // namespace extensions
