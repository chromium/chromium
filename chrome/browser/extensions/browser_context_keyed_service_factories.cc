// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_context_keyed_service_factories.h"

#include "chrome/browser/extensions/api/api_browser_context_keyed_service_factories.h"
#include "chrome/browser/extensions/chrome_browser_context_keyed_service_factories.h"

namespace chrome_extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  EnsureChromeBrowserContextKeyedServiceFactoriesBuilt();
  EnsureApiBrowserContextKeyedServiceFactoriesBuilt();
}

}  // namespace chrome_extensions
