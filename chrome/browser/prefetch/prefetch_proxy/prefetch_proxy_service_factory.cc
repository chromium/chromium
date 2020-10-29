// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
PrefetchProxyService* PrefetchProxyServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrefetchProxyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PrefetchProxyServiceFactory* PrefetchProxyServiceFactory::GetInstance() {
  return base::Singleton<PrefetchProxyServiceFactory>::get();
}

PrefetchProxyServiceFactory::PrefetchProxyServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrefetchProxyService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DataReductionProxyChromeSettingsFactory::GetInstance());
}

PrefetchProxyServiceFactory::~PrefetchProxyServiceFactory() = default;

KeyedService* PrefetchProxyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(!browser_context->IsOffTheRecord());
  return new PrefetchProxyService(Profile::FromBrowserContext(browser_context));
}
