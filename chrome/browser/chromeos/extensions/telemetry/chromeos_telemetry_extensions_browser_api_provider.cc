// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/chromeos_telemetry_extensions_browser_api_provider.h"

#include "chrome/browser/chromeos/extensions/telemetry/api/generated_api_registration.h"
#include "extensions/browser/extension_function_registry.h"

namespace chromeos {

ChromeOSTelemetryExtensionsBrowserAPIProvider::
    ChromeOSTelemetryExtensionsBrowserAPIProvider() = default;
ChromeOSTelemetryExtensionsBrowserAPIProvider::
    ~ChromeOSTelemetryExtensionsBrowserAPIProvider() = default;

void ChromeOSTelemetryExtensionsBrowserAPIProvider::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  api::ChromeOSGeneratedFunctionRegistry::RegisterAll(registry);
}

}  // namespace chromeos
