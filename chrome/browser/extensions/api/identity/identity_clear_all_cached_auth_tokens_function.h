// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CLEAR_ALL_CACHED_AUTH_TOKENS_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CLEAR_ALL_CACHED_AUTH_TOKENS_FUNCTION_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class IdentityClearAllCachedAuthTokensFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.clearAllCachedAuthTokens",
                             IDENTITY_CLEARALLCACHEDAUTHTOKENS)
  IdentityClearAllCachedAuthTokensFunction();

 private:
  ~IdentityClearAllCachedAuthTokensFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_CLEAR_ALL_CACHED_AUTH_TOKENS_FUNCTION_H_
