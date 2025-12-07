// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_unimplemented_functions_android.h"

#include "extensions/browser/extension_function.h"

DEFINE_UNIMPLEMENTED_EXTENSION_FUNCTION(
    IdentityClearAllCachedAuthTokensFunction,
    "identity.clearAllCachedAuthTokens")

DEFINE_UNIMPLEMENTED_EXTENSION_FUNCTION(IdentityGetAuthTokenFunction,
                                        "identity.getAuthToken")

DEFINE_UNIMPLEMENTED_EXTENSION_FUNCTION(IdentityRemoveCachedAuthTokenFunction,
                                        "identity.removeCachedAuthToken")
