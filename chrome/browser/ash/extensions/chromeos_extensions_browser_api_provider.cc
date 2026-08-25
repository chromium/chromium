// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/chromeos_extensions_browser_api_provider.h"

#include "chrome/browser/ash/extensions/api/generated_api_registration.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace ash {

ChromeOSExtensionsBrowserAPIProvider::ChromeOSExtensionsBrowserAPIProvider() =
    default;
ChromeOSExtensionsBrowserAPIProvider::~ChromeOSExtensionsBrowserAPIProvider() =
    default;

void ChromeOSExtensionsBrowserAPIProvider::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  extensions::api::ChromeOSGeneratedFunctionRegistry::RegisterAll(registry);
}

}  // namespace ash
