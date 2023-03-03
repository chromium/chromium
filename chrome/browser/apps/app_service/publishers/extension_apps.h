// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_H_

#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_base.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace extensions {
class Extension;
}

namespace apps {

class PublisherHost;

// An app publisher (in the App Service sense) of extension-backed apps for
// Chrome, including Chrome Apps (platform apps and legacy packaged apps) and
// hosted apps.
//
// See components/services/app_service/README.md.
class ExtensionApps : public apps::ExtensionAppsBase {
 public:
  explicit ExtensionApps(AppServiceProxy* proxy);
  ~ExtensionApps() override;

  ExtensionApps(const ExtensionApps&) = delete;
  ExtensionApps& operator=(const ExtensionApps&) = delete;

 private:
  friend class PublisherHost;

  // ExtensionAppsBase overrides.
  bool Accepts(const extensions::Extension* extension) override;
  bool ShouldShownInLauncher(const extensions::Extension* extension) override;
  AppPtr CreateApp(const extensions::Extension* extension,
                   Readiness readiness) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_H_
