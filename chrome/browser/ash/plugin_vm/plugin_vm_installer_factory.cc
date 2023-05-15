// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"

#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile.h"

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
    : ProfileKeyedServiceFactory(
          "PluginVmInstaller",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(BackgroundDownloadServiceFactory::GetInstance());
}

PluginVmInstallerFactory::~PluginVmInstallerFactory() = default;

KeyedService* PluginVmInstallerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PluginVmInstaller(Profile::FromBrowserContext(context));
}

}  // namespace plugin_vm
