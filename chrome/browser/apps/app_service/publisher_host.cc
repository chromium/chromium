// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publisher_host.h"

#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/web_applications/app_service/web_apps.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"
#include "chrome/browser/apps/app_service/publishers/bruschetta_apps.h"
#include "chrome/browser/apps/app_service/publishers/built_in_chromeos_apps.h"
#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"
#include "chrome/browser/apps/app_service/publishers/plugin_vm_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_apps.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#endif

namespace apps {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool g_omit_borealis_apps_for_testing_ = false;
bool g_omit_built_in_apps_for_testing_ = false;
bool g_omit_plugin_vm_apps_for_testing_ = false;
#endif

}  // anonymous namespace

PublisherHost::PublisherHost(AppServiceProxy* proxy) : proxy_(proxy) {
  DCHECK(proxy);
  Initialize();
}

PublisherHost::~PublisherHost() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
apps::StandaloneBrowserApps* PublisherHost::StandaloneBrowserApps() {
  return standalone_browser_apps_ ? standalone_browser_apps_.get() : nullptr;
}

void PublisherHost::SetArcIsRegistered() {
  chrome_apps_->ObserveArc();
}

void PublisherHost::ReInitializeCrostiniForTesting(AppServiceProxy* proxy) {
  DCHECK(proxy);
  crostini_apps_->Initialize();
}

void PublisherHost::RegisterPublishersForTesting() {
  DCHECK(proxy_);
  if (built_in_chrome_os_apps_) {
    proxy_->RegisterPublisher(AppType::kBuiltIn,
                              built_in_chrome_os_apps_.get());
  }
  if (crostini_apps_) {
    proxy_->RegisterPublisher(AppType::kCrostini, crostini_apps_.get());
  }
  if (chrome_apps_) {
    proxy_->RegisterPublisher(AppType::kChromeApp, chrome_apps_.get());
  }
  if (extension_apps_) {
    proxy_->RegisterPublisher(AppType::kExtension, extension_apps_.get());
  }
  if (plugin_vm_apps_) {
    proxy_->RegisterPublisher(AppType::kPluginVm, plugin_vm_apps_.get());
  }
  if (standalone_browser_apps_) {
    proxy_->RegisterPublisher(AppType::kStandaloneBrowser,
                              standalone_browser_apps_.get());
  }
  if (web_apps_) {
    proxy_->RegisterPublisher(AppType::kWeb, web_apps_.get());
  }
  if (borealis_apps_) {
    proxy_->RegisterPublisher(AppType::kBorealis, borealis_apps_.get());
  }
}

void PublisherHost::Shutdown() {
  chrome_apps_->Shutdown();
  if (extension_apps_) {
    extension_apps_->Shutdown();
  }
  if (web_apps_) {
    web_apps_->Shutdown();
  }
  borealis_apps_.reset();
}
#endif

void PublisherHost::Initialize() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* profile = proxy_->profile();
  if (!g_omit_built_in_apps_for_testing_) {
    built_in_chrome_os_apps_ = std::make_unique<BuiltInChromeOsApps>(proxy_);
    built_in_chrome_os_apps_->Initialize();
  }
  // TODO(b/170591339): Allow borealis to provide apps for the non-primary
  // profile.
  if (guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile) &&
      !g_omit_borealis_apps_for_testing_) {
    borealis_apps_ = std::make_unique<BorealisApps>(proxy_);
    borealis_apps_->Initialize();
  }

  bruschetta_apps_ = std::make_unique<BruschettaApps>(proxy_);
  bruschetta_apps_->Initialize();

  crostini_apps_ = std::make_unique<CrostiniApps>(proxy_);
  crostini_apps_->Initialize();

  chrome_apps_ =
      std::make_unique<ExtensionAppsChromeOs>(proxy_, AppType::kChromeApp);
  chrome_apps_->Initialize();

  extension_apps_ =
      std::make_unique<ExtensionAppsChromeOs>(proxy_, AppType::kExtension);
  extension_apps_->Initialize();

  if (!g_omit_plugin_vm_apps_for_testing_) {
    plugin_vm_apps_ = std::make_unique<PluginVmApps>(proxy_);
    plugin_vm_apps_->Initialize();
  }

  // Lacros does not support multi-signin, so only create for the primary
  // profile. This also avoids creating an instance for the lock screen app
  // profile and ensures there is only one instance of StandaloneBrowserApps.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      ash::ProfileHelper::IsPrimaryProfile(profile)) {
    standalone_browser_apps_ =
        std::make_unique<apps::StandaloneBrowserApps>(proxy_);
    standalone_browser_apps_->Initialize();
  }

  // `web_apps_` can be initialized itself.
  web_apps_ = std::make_unique<web_app::WebApps>(proxy_);
#else
  web_apps_ = std::make_unique<web_app::WebApps>(proxy_);

  chrome_apps_ = std::make_unique<ExtensionApps>(proxy_);
  chrome_apps_->Initialize();
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
ScopedOmitBorealisAppsForTesting::ScopedOmitBorealisAppsForTesting()
    : previous_omit_borealis_apps_for_testing_(
          g_omit_borealis_apps_for_testing_) {
  g_omit_borealis_apps_for_testing_ = true;
}

ScopedOmitBorealisAppsForTesting::~ScopedOmitBorealisAppsForTesting() {
  g_omit_borealis_apps_for_testing_ = previous_omit_borealis_apps_for_testing_;
}

ScopedOmitBuiltInAppsForTesting::ScopedOmitBuiltInAppsForTesting()
    : previous_omit_built_in_apps_for_testing_(
          g_omit_built_in_apps_for_testing_) {
  g_omit_built_in_apps_for_testing_ = true;
}

ScopedOmitBuiltInAppsForTesting::~ScopedOmitBuiltInAppsForTesting() {
  g_omit_built_in_apps_for_testing_ = previous_omit_built_in_apps_for_testing_;
}

ScopedOmitPluginVmAppsForTesting::ScopedOmitPluginVmAppsForTesting()
    : previous_omit_plugin_vm_apps_for_testing_(
          g_omit_plugin_vm_apps_for_testing_) {
  g_omit_plugin_vm_apps_for_testing_ = true;
}

ScopedOmitPluginVmAppsForTesting::~ScopedOmitPluginVmAppsForTesting() {
  g_omit_plugin_vm_apps_for_testing_ =
      previous_omit_plugin_vm_apps_for_testing_;
}
#endif

}  // namespace apps
