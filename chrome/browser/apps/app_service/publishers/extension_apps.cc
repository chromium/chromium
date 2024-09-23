// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps.h"

#include <memory>
#include <optional>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "extensions/common/extension.h"

namespace apps {

ExtensionApps::ExtensionApps(AppServiceProxy* proxy)
    : ExtensionAppsBase(proxy, AppType::kChromeApp) {}

ExtensionApps::~ExtensionApps() = default;

bool ExtensionApps::Accepts(const extensions::Extension* extension) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (extensions::IsExtensionUnsupportedDeprecatedApp(profile(),
                                                      extension->id())) {
    return false;
  }
#endif
  if (!extension->is_app()) {
    return false;
  }

  return true;
}

bool ExtensionApps::ShouldShownInLauncher(
    const extensions::Extension* extension) {
  return true;
}

AppPtr ExtensionApps::CreateApp(const extensions::Extension* extension,
                                Readiness readiness) {
  auto app = CreateAppImpl(extension, readiness);
  app->icon_key = IconKey(GetIconEffects(extension));
  app->has_badge = false;
  app->paused = false;
  return app;
}

}  // namespace apps
