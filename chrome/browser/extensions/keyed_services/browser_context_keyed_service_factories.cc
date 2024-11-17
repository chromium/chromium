// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/keyed_services/browser_context_keyed_service_factories.h"

#include "chrome/browser/extensions/api/api_browser_context_keyed_service_factories.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/keyed_services/chrome_browser_context_keyed_service_factories.h"
#endif

namespace chrome_extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EnsureChromeBrowserContextKeyedServiceFactoriesBuilt();
#endif
  EnsureApiBrowserContextKeyedServiceFactoriesBuilt();
}

}  // namespace chrome_extensions
