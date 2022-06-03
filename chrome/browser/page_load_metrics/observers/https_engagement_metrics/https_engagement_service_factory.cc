// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
HttpsEngagementService* HttpsEngagementServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<HttpsEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HttpsEngagementServiceFactory* HttpsEngagementServiceFactory::GetInstance() {
  return base::Singleton<HttpsEngagementServiceFactory>::get();
}

HttpsEngagementServiceFactory::HttpsEngagementServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HttpEngagementKeyService",
          BrowserContextDependencyManager::GetInstance()) {}

HttpsEngagementServiceFactory::~HttpsEngagementServiceFactory() {}

KeyedService* HttpsEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new HttpsEngagementService();
}

content::BrowserContext* HttpsEngagementServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool HttpsEngagementServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
