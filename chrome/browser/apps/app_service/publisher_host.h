// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace web_app {
class WebApps;
}  // namespace web_app

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class BuiltInChromeOsApps;
class CrostiniApps;
class ExtensionAppsChromeOs;
class PluginVmApps;
class StandaloneBrowserApps;
class BorealisApps;
class InstanceRegistry;
class BrowserAppInstanceRegistry;
#else
class ExtensionApps;
#endif

// PublisherHost saves publishers created by AppServiceProxy for the ash side
// Chrome OS and other platforms, and excludes the Lacros side, because
// AppServiceProxy in Lacros doesn't have/create any publisher.
class PublisherHost {
 public:
  explicit PublisherHost(
      Profile* profile,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      InstanceRegistry* instance_registry,
      BrowserAppInstanceRegistry* browser_app_instance_registry,
#endif
      const mojo::Remote<apps::mojom::AppService>& app_service);

  PublisherHost(const PublisherHost&) = delete;
  PublisherHost& operator=(const PublisherHost&) = delete;
  ~PublisherHost();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetArcIsRegistered();

  void FlushMojoCallsForTesting();

  void ReInitializeCrostiniForTesting(Profile* profile);

  void Shutdown();
#endif

 private:
  void Initialize();

  Profile* profile_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<BuiltInChromeOsApps> built_in_chrome_os_apps_;
  std::unique_ptr<CrostiniApps> crostini_apps_;
  std::unique_ptr<ExtensionAppsChromeOs> extension_apps_;
  std::unique_ptr<PluginVmApps> plugin_vm_apps_;
  std::unique_ptr<StandaloneBrowserApps> standalone_browser_apps_;
  std::unique_ptr<web_app::WebApps> web_apps_;
  std::unique_ptr<BorealisApps> borealis_apps_;

  InstanceRegistry* instance_registry_ = nullptr;
  BrowserAppInstanceRegistry* browser_app_instance_registry_ = nullptr;
#else
  std::unique_ptr<web_app::WebApps> web_apps_;
  std::unique_ptr<ExtensionApps> extension_apps_;
#endif

  const mojo::Remote<apps::mojom::AppService>& app_service_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ScopedOmitBuiltInAppsForTesting {
 public:
  ScopedOmitBuiltInAppsForTesting();
  ScopedOmitBuiltInAppsForTesting(const ScopedOmitBuiltInAppsForTesting&) =
      delete;
  ScopedOmitBuiltInAppsForTesting& operator=(
      const ScopedOmitBuiltInAppsForTesting&) = delete;
  ~ScopedOmitBuiltInAppsForTesting();

 private:
  const bool previous_omit_built_in_apps_for_testing_;
};

class ScopedOmitPluginVmAppsForTesting {
 public:
  ScopedOmitPluginVmAppsForTesting();
  ScopedOmitPluginVmAppsForTesting(const ScopedOmitPluginVmAppsForTesting&) =
      delete;
  ScopedOmitPluginVmAppsForTesting& operator=(
      const ScopedOmitPluginVmAppsForTesting&) = delete;
  ~ScopedOmitPluginVmAppsForTesting();

 private:
  const bool previous_omit_plugin_vm_apps_for_testing_;
};
#endif

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_
