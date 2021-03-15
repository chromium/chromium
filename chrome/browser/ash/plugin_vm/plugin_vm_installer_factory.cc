// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"

#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace plugin_vm {

// static
PluginVmInstaller* PluginVmInstallerFactory::GetForProfile(Profile* profile) {
  return static_cast<PluginVmInstaller*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PluginVmInstallerFactory* PluginVmInstallerFactory::GetInstance() {
  return base::Singleton<PluginVmInstallerFactory>::get();
}

PluginVmInstallerFactory::PluginVmInstallerFactory()
    : BrowserContextKeyedServiceFactory(
          "PluginVmInstaller",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
}

PluginVmInstallerFactory::~PluginVmInstallerFactory() = default;

KeyedService* PluginVmInstallerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PluginVmInstaller(Profile::FromBrowserContext(context));
}

content::BrowserContext* PluginVmInstallerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace plugin_vm
