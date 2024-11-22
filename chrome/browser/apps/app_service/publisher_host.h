// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHER_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"

namespace web_app {
class WebApps;
}  // namespace web_app

namespace apps {

#if BUILDFLAG(IS_CHROMEOS)
class BorealisApps;
class BruschettaApps;
class CrostiniApps;
class ExtensionAppsChromeOs;
class PluginVmApps;
#else
class ExtensionApps;
#endif

// PublisherHost saves publishers created by AppServiceProxy.
class PublisherHost {
 public:
  explicit PublisherHost(AppServiceProxy* proxy);
  PublisherHost(const PublisherHost&) = delete;
  PublisherHost& operator=(const PublisherHost&) = delete;
  ~PublisherHost();

#if BUILDFLAG(IS_CHROMEOS)
  void SetArcIsRegistered();

  void ReInitializeCrostiniForTesting(AppServiceProxy* proxy);

  void RegisterPublishersForTesting();

  void Shutdown();
#endif

 private:
  void Initialize();

  // Owns this class.
  raw_ptr<AppServiceProxy> proxy_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<BorealisApps> borealis_apps_;
  std::unique_ptr<BruschettaApps> bruschetta_apps_;
  std::unique_ptr<CrostiniApps> crostini_apps_;
  std::unique_ptr<ExtensionAppsChromeOs> chrome_apps_;
  std::unique_ptr<ExtensionAppsChromeOs> extension_apps_;
  std::unique_ptr<PluginVmApps> plugin_vm_apps_;
  std::unique_ptr<web_app::WebApps> web_apps_;
#else
  std::unique_ptr<web_app::WebApps> web_apps_;
  std::unique_ptr<ExtensionApps> chrome_apps_;
#endif
};

#if BUILDFLAG(IS_CHROMEOS)
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
