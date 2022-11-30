// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_private_api.h"

#include "chrome/browser/extensions/api/identity/identity_api.h"

namespace extensions {

IdentityPrivateSetConsentResultFunction::
    IdentityPrivateSetConsentResultFunction() = default;
IdentityPrivateSetConsentResultFunction::
    ~IdentityPrivateSetConsentResultFunction() = default;

ExtensionFunction::ResponseAction
IdentityPrivateSetConsentResultFunction::Run() {
  std::unique_ptr<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  IdentityAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->SetConsentResult(params->result, params->window_id);

  return RespondNow(NoArguments());
}

}  // namespace extensions
