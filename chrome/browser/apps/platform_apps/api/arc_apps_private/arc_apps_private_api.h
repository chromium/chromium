// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_ARC_APPS_PRIVATE_ARC_APPS_PRIVATE_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_ARC_APPS_PRIVATE_ARC_APPS_PRIVATE_API_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace chrome_apps {
namespace api {

class ArcAppsPrivateAPI : public extensions::BrowserContextKeyedAPI,
                          public extensions::EventRouter::Observer,
                          public ArcAppListPrefs::Observer {
 public:
  static extensions::BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>*
  GetFactoryInstance();

  explicit ArcAppsPrivateAPI(content::BrowserContext* context);
  ~ArcAppsPrivateAPI() override;

  // extensions::BrowserContextKeyedAPI:
  void Shutdown() override;

  // extensions::EventRouter::Observer:
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;
  void OnListenerRemoved(const extensions::EventListenerInfo& details) override;

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>;

  static const char* service_name() { return "ArcAppsPrivateAPI"; }

  // extensions::BrowserContextKeyedAPI:
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const context_;

  ScopedObserver<ArcAppListPrefs, ArcAppListPrefs::Observer>
      scoped_prefs_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateAPI);
};

class ArcAppsPrivateGetLaunchableAppsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.getLaunchableApps",
                             ARCAPPSPRIVATE_GETLAUNCHABLEAPPS)

  ArcAppsPrivateGetLaunchableAppsFunction();

 protected:
  ~ArcAppsPrivateGetLaunchableAppsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateGetLaunchableAppsFunction);
};

class ArcAppsPrivateLaunchAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.launchApp",
                             ARCAPPSPRIVATE_LAUNCHAPP)

  ArcAppsPrivateLaunchAppFunction();

 protected:
  ~ArcAppsPrivateLaunchAppFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateLaunchAppFunction);
};

}  // namespace api
}  // namespace chrome_apps

template <>
struct extensions::BrowserContextFactoryDependencies<
    chrome_apps::api::ArcAppsPrivateAPI> {
  static void DeclareFactoryDependencies(
      extensions::BrowserContextKeyedAPIFactory<
          chrome_apps::api::ArcAppsPrivateAPI>* factory) {
    factory->DependsOn(
        ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
    factory->DependsOn(ArcAppListPrefsFactory::GetInstance());
  }
};

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_ARC_APPS_PRIVATE_ARC_APPS_PRIVATE_API_H_
