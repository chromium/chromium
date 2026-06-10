// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_DEPRECATION_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_DEPRECATION_H_

#include <string>

#include "build/build_config.h"

namespace content {
class BrowserContext;
}

namespace extensions {

namespace testing {
// Because the allow-list needs to stick around for a while, this flag makes it
// easy for us to continue testing chrome apps on Windows/Mac/Linux without
// having to jump through hurdles to add ids to the allow-list.
// TODO(http://b/268221237): Remove this & tests on WML once allow-list is
// removed.
extern bool g_enable_chrome_apps_for_testing;
}  // namespace testing

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Returns whether the extension with `extension_id` is an unsupported
// deprecated app (hosted app, legacy packaged app, or platform app) on
// Windows/Mac/Linux.
bool IsExtensionUnsupportedDeprecatedApp(content::BrowserContext* context,
                                         const std::string& extension_id);
#endif

namespace chrome_app_deprecation {

// Returns if the app is managed by extension default apps.
bool IsPreinstalledAppId(const std::string& app_id);

void SetPreinstalledAppIdForTesting(const char* app_id);

}  // namespace chrome_app_deprecation
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_DEPRECATION_H_
