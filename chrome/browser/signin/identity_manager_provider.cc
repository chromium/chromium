// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_provider.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace signin {

namespace {

IdentityManagerProvider& GetIdentityManagerProvider() {
  static base::NoDestructor<IdentityManagerProvider> provider;
  return *provider;
}

}  // namespace

void SetIdentityManagerProvider(const IdentityManagerProvider& provider) {
  IdentityManagerProvider& instance = GetIdentityManagerProvider();

  // Exactly one of `provider` or `instance` should be non-null.
  if (provider)
    DCHECK(!instance);
  else
    DCHECK(instance);

  instance = provider;
}

IdentityManager* GetIdentityManagerForBrowserContext(
    content::BrowserContext* context) {
  return GetIdentityManagerProvider().Run(context);
}

}  // namespace signin
