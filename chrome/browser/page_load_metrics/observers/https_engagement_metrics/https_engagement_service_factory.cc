// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service.h"

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
    : ProfileKeyedServiceFactory(
          "HttpEngagementKeyService",
          ProfileSelections::BuildRedirectedInIncognito()) {}

HttpsEngagementServiceFactory::~HttpsEngagementServiceFactory() {}

KeyedService* HttpsEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new HttpsEngagementService();
}

bool HttpsEngagementServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
