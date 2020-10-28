// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/isolated_prerender_service_factory.h"

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prefetch/prefetch_proxy/isolated_prerender_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
IsolatedPrerenderService* IsolatedPrerenderServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IsolatedPrerenderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
IsolatedPrerenderServiceFactory*
IsolatedPrerenderServiceFactory::GetInstance() {
  return base::Singleton<IsolatedPrerenderServiceFactory>::get();
}

IsolatedPrerenderServiceFactory::IsolatedPrerenderServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "IsolatedPrerenderService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DataReductionProxyChromeSettingsFactory::GetInstance());
}

IsolatedPrerenderServiceFactory::~IsolatedPrerenderServiceFactory() = default;

KeyedService* IsolatedPrerenderServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(!browser_context->IsOffTheRecord());
  return new IsolatedPrerenderService(
      Profile::FromBrowserContext(browser_context));
}
