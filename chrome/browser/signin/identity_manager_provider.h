// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_
#define CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_

#include "base/functional/callback.h"

namespace content {
class BrowserContext;
}

namespace signin {

class IdentityManager;

using IdentityManagerProvider =
    base::RepeatingCallback<IdentityManager*(content::BrowserContext*)>;

// Called by IdentityManagerFactory to expose a way to retrieve the
// IdentityManager for a specific BrowserContext/Profile. This exists so that
// components which don't depend on //chrome/browser can still access the
// IdentityManager.
void SetIdentityManagerProvider(const IdentityManagerProvider& provider);

IdentityManager* GetIdentityManagerForBrowserContext(
    content::BrowserContext* context);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_
