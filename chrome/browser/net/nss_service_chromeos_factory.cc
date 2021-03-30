// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service_chromeos_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/net/nss_service_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

NssServiceChromeOS* NssServiceChromeOSFactory::GetForContext(
    content::BrowserContext* browser_context) {
  return static_cast<NssServiceChromeOS*>(
      GetInstance().GetServiceForBrowserContext(browser_context, true));
}

NssServiceChromeOSFactory::NssServiceChromeOSFactory()
    : BrowserContextKeyedServiceFactory(
          "NssServiceChromeOSFactory",
          BrowserContextDependencyManager::GetInstance()) {}

NssServiceChromeOSFactory::~NssServiceChromeOSFactory() = default;

NssServiceChromeOSFactory& NssServiceChromeOSFactory::GetInstance() {
  static base::NoDestructor<NssServiceChromeOSFactory> instance;
  return *instance;
}

KeyedService* NssServiceChromeOSFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new NssServiceChromeOS(Profile::FromBrowserContext(profile));
}

content::BrowserContext* NssServiceChromeOSFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Create separate service for incognito profiles.
  return context;
}
