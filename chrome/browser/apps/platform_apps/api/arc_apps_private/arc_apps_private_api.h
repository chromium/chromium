// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_ARC_APPS_PRIVATE_ARC_APPS_PRIVATE_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_ARC_APPS_PRIVATE_ARC_APPS_PRIVATE_API_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
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
  ArcAppsPrivateAPI(const ArcAppsPrivateAPI&) = delete;
  ArcAppsPrivateAPI& operator=(const ArcAppsPrivateAPI&) = delete;
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

  const raw_ptr<content::BrowserContext> context_;

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      scoped_prefs_observation_{this};
};

class ArcAppsPrivateGetLaunchableAppsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.getLaunchableApps",
                             ARCAPPSPRIVATE_GETLAUNCHABLEAPPS)

  ArcAppsPrivateGetLaunchableAppsFunction();
  ArcAppsPrivateGetLaunchableAppsFunction(
      const ArcAppsPrivateGetLaunchableAppsFunction&) = delete;
  ArcAppsPrivateGetLaunchableAppsFunction& operator=(
      const ArcAppsPrivateGetLaunchableAppsFunction&) = delete;

 protected:
  ~ArcAppsPrivateGetLaunchableAppsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ArcAppsPrivateLaunchAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("arcAppsPrivate.launchApp",
                             ARCAPPSPRIVATE_LAUNCHAPP)

  ArcAppsPrivateLaunchAppFunction();
  ArcAppsPrivateLaunchAppFunction(const ArcAppsPrivateLaunchAppFunction&) =
      delete;
  ArcAppsPrivateLaunchAppFunction& operator=(
      const ArcAppsPrivateLaunchAppFunction&) = delete;

 protected:
  ~ArcAppsPrivateLaunchAppFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
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
