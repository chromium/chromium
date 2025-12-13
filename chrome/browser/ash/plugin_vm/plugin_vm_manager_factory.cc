// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"

namespace plugin_vm {

PluginVmManager* PluginVmManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<PluginVmManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PluginVmManagerFactory* PluginVmManagerFactory::GetInstance() {
  static base::NoDestructor<PluginVmManagerFactory> factory;
  return factory.get();
}

PluginVmManagerFactory::PluginVmManagerFactory()
    : ProfileKeyedServiceFactory(
          "PluginVmManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

PluginVmManagerFactory::~PluginVmManagerFactory() = default;

// BrowserContextKeyedServiceFactory:
std::unique_ptr<KeyedService>
PluginVmManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* application_locale_storage =
      g_browser_process->GetFeatures()->application_locale_storage();
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PluginVmManagerImpl>(application_locale_storage,
                                               profile);
}

}  // namespace plugin_vm
