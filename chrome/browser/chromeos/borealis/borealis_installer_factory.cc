// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_installer_factory.h"

#include "chrome/browser/chromeos/borealis/borealis_installer_impl.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace borealis {

// static
BorealisInstaller* BorealisInstallerFactory::GetForProfile(Profile* profile) {
  return static_cast<BorealisInstaller*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BorealisInstallerFactory* BorealisInstallerFactory::GetInstance() {
  static base::NoDestructor<BorealisInstallerFactory> factory;
  return factory.get();
}

BorealisInstallerFactory::BorealisInstallerFactory()
    : BrowserContextKeyedServiceFactory(
          "BorealisInstaller",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
}

BorealisInstallerFactory::~BorealisInstallerFactory() = default;

KeyedService* BorealisInstallerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BorealisInstallerImpl(Profile::FromBrowserContext(context));
}

content::BrowserContext* BorealisInstallerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace borealis
