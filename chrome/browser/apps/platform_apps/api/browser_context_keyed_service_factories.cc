// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser_context_keyed_service_factories.h"

#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/platform_apps/api/easy_unlock_private/easy_unlock_private_api.h"
#endif

namespace chrome_apps {
namespace api {

void EnsureAPIBrowserContextKeyedServiceFactoriesBuilt() {
#if defined(OS_CHROMEOS)
  EasyUnlockPrivateAPI::GetFactoryInstance();
#endif
}

}  // namespace api
}  // namespace chrome_apps
