// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_CHROMEOS_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_CHROMEOS_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "build/build_config.h"
#include "extensions/browser/extensions_browser_api_provider.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(IS_CHROMEOS));
static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace ash {

class ChromeOSExtensionsBrowserAPIProvider
    : public extensions::ExtensionsBrowserAPIProvider {
 public:
  ChromeOSExtensionsBrowserAPIProvider();
  ChromeOSExtensionsBrowserAPIProvider(
      const ChromeOSExtensionsBrowserAPIProvider&) = delete;
  ChromeOSExtensionsBrowserAPIProvider& operator=(
      const ChromeOSExtensionsBrowserAPIProvider&) = delete;

  ~ChromeOSExtensionsBrowserAPIProvider() override;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_CHROMEOS_EXTENSIONS_BROWSER_API_PROVIDER_H_
