// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_UNIMPLEMENTED_FUNCTIONS_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_UNIMPLEMENTED_FUNCTIONS_ANDROID_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

DECLARE_UNIMPLEMENTED_EXTENSION_FUNCTION(
    IdentityClearAllCachedAuthTokensFunction,
    "identity.clearAllCachedAuthTokens",
    IDENTITY_CLEARALLCACHEDAUTHTOKENS);

DECLARE_UNIMPLEMENTED_EXTENSION_FUNCTION(IdentityGetAuthTokenFunction,
                                         "identity.getAuthToken",
                                         EXPERIMENTAL_IDENTITY_GETAUTHTOKEN);

DECLARE_UNIMPLEMENTED_EXTENSION_FUNCTION(
  IdentityLaunchWebAuthFlowFunction,
  "identity.launchWebAuthFlow",
  EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW);

DECLARE_UNIMPLEMENTED_EXTENSION_FUNCTION(
    IdentityRemoveCachedAuthTokenFunction,
    "identity.removeCachedAuthToken",
    EXPERIMENTAL_IDENTITY_REMOVECACHEDAUTHTOKEN);

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_UNIMPLEMENTED_FUNCTIONS_ANDROID_H_
