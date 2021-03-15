// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"

#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          "PluginVmManager",
          BrowserContextDependencyManager::GetInstance()) {}

PluginVmManagerFactory::~PluginVmManagerFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* PluginVmManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PluginVmManagerImpl(profile);
}

}  // namespace plugin_vm
