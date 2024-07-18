// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"

namespace web_app {
class WebApps;
}  // namespace web_app

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class BorealisApps;
class BruschettaApps;
class BuiltInChromeOsApps;
class CrostiniApps;
class ExtensionAppsChromeOs;
class PluginVmApps;
class StandaloneBrowserApps;
#else
class ExtensionApps;
#endif

// PublisherHost saves publishers created by AppServiceProxy for the ash side
// Chrome OS and other platforms, and excludes the Lacros side, because
// AppServiceProxy in Lacros doesn't have/create any publisher.
class PublisherHost {
 public:
  explicit PublisherHost(AppServiceProxy* proxy);
  PublisherHost(const PublisherHost&) = delete;
  PublisherHost& operator=(const PublisherHost&) = delete;
  ~PublisherHost();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::StandaloneBrowserApps* StandaloneBrowserApps();

  void SetArcIsRegistered();

  void ReInitializeCrostiniForTesting(AppServiceProxy* proxy);

  void RegisterPublishersForTesting();

  void Shutdown();
#endif

 private:
  void Initialize();

  // Owns this class.
  raw_ptr<AppServiceProxy> proxy_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<BorealisApps> borealis_apps_;
  std::unique_ptr<BruschettaApps> bruschetta_apps_;
  std::unique_ptr<BuiltInChromeOsApps> built_in_chrome_os_apps_;
  std::unique_ptr<CrostiniApps> crostini_apps_;
  std::unique_ptr<ExtensionAppsChromeOs> chrome_apps_;
  std::unique_ptr<ExtensionAppsChromeOs> extension_apps_;
  std::unique_ptr<PluginVmApps> plugin_vm_apps_;
  std::unique_ptr<apps::StandaloneBrowserApps> standalone_browser_apps_;
  std::unique_ptr<web_app::WebApps> web_apps_;
#else
  std::unique_ptr<web_app::WebApps> web_apps_;
  std::unique_ptr<ExtensionApps> chrome_apps_;
#endif
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ScopedOmitBorealisAppsForTesting {
 public:
  ScopedOmitBorealisAppsForTesting();
  ScopedOmitBorealisAppsForTesting(const ScopedOmitBorealisAppsForTesting&) =
      delete;
  ScopedOmitBorealisAppsForTesting& operator=(
      const ScopedOmitBorealisAppsForTesting&) = delete;
  ~ScopedOmitBorealisAppsForTesting();

 private:
  const bool previous_omit_borealis_apps_for_testing_;
};

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
