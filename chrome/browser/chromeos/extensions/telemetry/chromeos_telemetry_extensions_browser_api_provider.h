// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_CHROMEOS_TELEMETRY_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_CHROMEOS_TELEMETRY_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "extensions/browser/extensions_browser_api_provider.h"

class ExtensionFunctionRegistry;

namespace chromeos {

class ChromeOSTelemetryExtensionsBrowserAPIProvider
    : public extensions::ExtensionsBrowserAPIProvider {
 public:
  ChromeOSTelemetryExtensionsBrowserAPIProvider();
  ChromeOSTelemetryExtensionsBrowserAPIProvider(
      const ChromeOSTelemetryExtensionsBrowserAPIProvider&) = delete;
  ChromeOSTelemetryExtensionsBrowserAPIProvider& operator=(
      const ChromeOSTelemetryExtensionsBrowserAPIProvider&) = delete;
  ~ChromeOSTelemetryExtensionsBrowserAPIProvider() override;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_CHROMEOS_TELEMETRY_EXTENSIONS_BROWSER_API_PROVIDER_H_
