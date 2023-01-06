// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/extension_uninstaller.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"

namespace apps {

ExtensionApps::ExtensionApps(AppServiceProxy* proxy, AppType app_type)
    : ExtensionAppsBase(proxy, app_type) {}

ExtensionApps::~ExtensionApps() = default;

// static
void ExtensionApps::UninstallImpl(Profile* profile,
                                  const std::string& app_id,
                                  gfx::NativeWindow parent_window) {
  if (!profile) {
    return;
  }

  ExtensionUninstaller::Create(profile, app_id, parent_window);
}

bool ExtensionApps::Accepts(const extensions::Extension* extension) {
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
  app->icon_key =
      std::move(*icon_key_factory().CreateIconKey(GetIconEffects(extension)));
  app->has_badge = false;
  app->paused = false;
  return app;
}

}  // namespace apps
