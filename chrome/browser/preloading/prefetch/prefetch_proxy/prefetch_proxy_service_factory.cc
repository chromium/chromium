// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/profiles/profile.h"
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
    : ProfileKeyedServiceFactory("PrefetchProxyService") {}

PrefetchProxyServiceFactory::~PrefetchProxyServiceFactory() = default;

KeyedService* PrefetchProxyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(!browser_context->IsOffTheRecord());
  return new PrefetchProxyService(Profile::FromBrowserContext(browser_context));
}
