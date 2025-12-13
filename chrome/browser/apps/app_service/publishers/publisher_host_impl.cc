// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/publisher_host_impl.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/web_applications/app_service/web_apps.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"
#include "chrome/browser/apps/app_service/publishers/bruschetta_apps.h"
#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"
#include "chrome/browser/apps/app_service/publishers/plugin_vm_apps.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
bool g_omit_borealis_apps_for_testing_ = false;
bool g_omit_plugin_vm_apps_for_testing_ = false;

bool IsKioskSessionProfile(Profile* profile) {
  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  return user != nullptr && user->IsKioskType();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // anonymous namespace

PublisherHostImpl::PublisherHostImpl(AppServiceProxy* proxy) : proxy_(proxy) {
  DCHECK(proxy);
  Initialize();
}

PublisherHostImpl::~PublisherHostImpl() = default;

#if BUILDFLAG(IS_CHROMEOS)
void PublisherHostImpl::SetArcIsRegistered() {
  chrome_apps_->ObserveArc();
}

void PublisherHostImpl::ReInitializeCrostiniForTesting() {
  crostini_apps_->Initialize();
}

void PublisherHostImpl::RegisterPublishersForTesting() {
  DCHECK(proxy_);
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
  if (web_apps_) {
    proxy_->RegisterPublisher(AppType::kWeb, web_apps_.get());
  }
  if (borealis_apps_) {
    proxy_->RegisterPublisher(AppType::kBorealis, borealis_apps_.get());
  }
}

void PublisherHostImpl::Shutdown() {
  chrome_apps_->Shutdown();
  if (extension_apps_) {
    extension_apps_->Shutdown();
  }
  if (web_apps_) {
    web_apps_->Shutdown();
  }
  borealis_apps_.reset();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void PublisherHostImpl::Initialize() {
#if BUILDFLAG(IS_CHROMEOS)
  auto* profile = proxy_->profile();
  // GuestOS and PluginVm apps are not available in kiosk mode.
  if (!IsKioskSessionProfile(profile)) {
    // TODO(crbug.com/170591339): Allow borealis to provide apps for the
    // non-primary profile.
    if (guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile) &&
        !g_omit_borealis_apps_for_testing_) {
      borealis_apps_ = std::make_unique<BorealisApps>(proxy_);
      borealis_apps_->Initialize();
    }

    bruschetta_apps_ = std::make_unique<BruschettaApps>(proxy_);
    bruschetta_apps_->Initialize();

    crostini_apps_ = std::make_unique<CrostiniApps>(proxy_);
    crostini_apps_->Initialize();

    if (!g_omit_plugin_vm_apps_for_testing_) {
      plugin_vm_apps_ = std::make_unique<PluginVmApps>(proxy_);
      plugin_vm_apps_->Initialize();
    }
  }

  chrome_apps_ =
      std::make_unique<ExtensionAppsChromeOs>(proxy_, AppType::kChromeApp);
  chrome_apps_->Initialize();

  extension_apps_ =
      std::make_unique<ExtensionAppsChromeOs>(proxy_, AppType::kExtension);
  extension_apps_->Initialize();

  // `web_apps_` can be initialized itself.
  web_apps_ = std::make_unique<web_app::WebApps>(proxy_);
#else
  web_apps_ = std::make_unique<web_app::WebApps>(proxy_);

  chrome_apps_ = std::make_unique<ExtensionApps>(proxy_);
  chrome_apps_->Initialize();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
ScopedOmitBorealisAppsForTesting::ScopedOmitBorealisAppsForTesting()
    : previous_omit_borealis_apps_for_testing_(
          g_omit_borealis_apps_for_testing_) {
  g_omit_borealis_apps_for_testing_ = true;
}

ScopedOmitBorealisAppsForTesting::~ScopedOmitBorealisAppsForTesting() {
  g_omit_borealis_apps_for_testing_ = previous_omit_borealis_apps_for_testing_;
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
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace apps
