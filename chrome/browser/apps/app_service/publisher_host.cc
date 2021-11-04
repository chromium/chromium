// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publisher_host.h"

#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/web_applications/app_service/web_apps.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"
#include "chrome/browser/apps/app_service/publishers/built_in_chromeos_apps.h"
#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"
#include "chrome/browser/apps/app_service/publishers/plugin_vm_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_apps.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#endif

namespace apps {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
void PublisherHost::SetArcIsRegistered() {
  extension_apps_->ObserveArc();
}

void PublisherHost::FlushMojoCallsForTesting() {
  if (built_in_chrome_os_apps_) {
    built_in_chrome_os_apps_->FlushMojoCallsForTesting();
  }
  crostini_apps_->FlushMojoCallsForTesting();
  extension_apps_->FlushMojoCallsForTesting();
  if (plugin_vm_apps_)
    plugin_vm_apps_->FlushMojoCallsForTesting();
  if (standalone_browser_apps_) {
    standalone_browser_apps_->FlushMojoCallsForTesting();
  }
  if (web_apps_) {
    web_apps_->FlushMojoCallsForTesting();
  }
  if (borealis_apps_) {
    borealis_apps_->FlushMojoCallsForTesting();
  }
}

void PublisherHost::ReInitializeCrostiniForTesting(AppServiceProxy* proxy) {
  DCHECK(proxy);
  crostini_apps_->Initialize(proxy->AppService());
}

void PublisherHost::Shutdown() {
  if (proxy_->AppService().is_connected()) {
    extension_apps_->Shutdown();
    if (web_apps_) {
      web_apps_->Shutdown();
    }
  }
  borealis_apps_.reset();
}
#endif

void PublisherHost::Initialize() {
  auto* profile = proxy_->profile();
  auto& app_service = proxy_->AppService();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!g_omit_built_in_apps_for_testing_) {
    built_in_chrome_os_apps_ = std::make_unique<BuiltInChromeOsApps>(proxy_);
  }
  // TODO(b/170591339): Allow borealis to provide apps for the non-primary
  // profile.
  if (guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)) {
    borealis_apps_ = std::make_unique<BorealisApps>(proxy_);
  }
  crostini_apps_ = std::make_unique<CrostiniApps>(proxy_);
  extension_apps_ = std::make_unique<ExtensionAppsChromeOs>(
      app_service, profile, &proxy_->InstanceRegistry());
  if (!g_omit_plugin_vm_apps_for_testing_) {
    plugin_vm_apps_ = std::make_unique<PluginVmApps>(proxy_);
  }
  // Lacros does not support multi-signin, so only create for the primary
  // profile. This also avoids creating an instance for the lock screen app
  // profile and ensures there is only one instance of StandaloneBrowserApps.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    standalone_browser_apps_ = std::make_unique<StandaloneBrowserApps>(
        app_service, profile, proxy_->BrowserAppInstanceRegistry());
  }
  web_apps_ = std::make_unique<web_app::WebApps>(
      app_service, &proxy_->InstanceRegistry(), profile);
#else
  web_apps_ = std::make_unique<web_app::WebApps>(app_service, profile);
  extension_apps_ = std::make_unique<ExtensionApps>(app_service, profile);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
