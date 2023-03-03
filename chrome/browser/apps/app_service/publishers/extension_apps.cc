// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps.h"

#include <memory>
#include <utility>

#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "extensions/common/extension.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

ExtensionApps::ExtensionApps(AppServiceProxy* proxy)
    : ExtensionAppsBase(proxy, AppType::kChromeApp) {}

ExtensionApps::~ExtensionApps() = default;

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
