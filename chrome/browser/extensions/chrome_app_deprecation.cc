// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_deprecation.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace testing {
bool g_enable_chrome_apps_for_testing = false;
}  // namespace testing

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
bool IsExtensionUnsupportedDeprecatedApp(content::BrowserContext* context,
                                         const std::string& extension_id) {
  if (testing::g_enable_chrome_apps_for_testing) {
    return false;
  }

  if (extension_id == extensions::kWebStoreAppId) {
    return false;
  }

  auto* registry = ExtensionRegistry::Get(context);
  // May be nullptr in unit tests.
  if (!registry) {
    return false;
  }

  const extensions::Extension* app = registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!app || !app->is_app()) {
    return false;
  }

  return true;
}
#endif

namespace chrome_app_deprecation {

const char* g_preinstalled_app_for_testing = nullptr;

bool IsPreinstalledAppId(const std::string& app_id) {
  if (g_preinstalled_app_for_testing &&
      app_id == g_preinstalled_app_for_testing) {
    return true;
  }
  return extension_misc::IsPreinstalledAppId(app_id);
}

void SetPreinstalledAppIdForTesting(const char* app_id) {
  g_preinstalled_app_for_testing = app_id;
}

}  // namespace chrome_app_deprecation
}  // namespace extensions
