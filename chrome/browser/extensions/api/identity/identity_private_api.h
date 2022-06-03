// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_PRIVATE_API_H_

#include "chrome/common/extensions/api/identity_private.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class IdentityPrivateSetConsentResultFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identityPrivate.setConsentResult",
                             IDENTITYPRIVATE_SETCONSENTRESULT)

  IdentityPrivateSetConsentResultFunction();

 private:
  using Params = api::identity_private::SetConsentResult::Params;
  ~IdentityPrivateSetConsentResultFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_PRIVATE_API_H_
