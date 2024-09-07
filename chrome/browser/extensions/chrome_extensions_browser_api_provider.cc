// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_api_provider.h"

#include "chrome/browser/extensions/api/commands/commands.h"
#include "chrome/browser/extensions/api/generated_api_registration.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

ChromeExtensionsBrowserAPIProvider::ChromeExtensionsBrowserAPIProvider() =
    default;
ChromeExtensionsBrowserAPIProvider::~ChromeExtensionsBrowserAPIProvider() =
    default;

void ChromeExtensionsBrowserAPIProvider::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  // Commands
  registry->RegisterFunction<GetAllCommandsFunction>();

  // Generated APIs from Chrome.
  api::ChromeGeneratedFunctionRegistry::RegisterAll(registry);
}

}  // namespace extensions
