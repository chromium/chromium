// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service.h"

// static
HttpsEngagementService* HttpsEngagementServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<HttpsEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HttpsEngagementServiceFactory* HttpsEngagementServiceFactory::GetInstance() {
  static base::NoDestructor<HttpsEngagementServiceFactory> instance;
  return instance.get();
}

HttpsEngagementServiceFactory::HttpsEngagementServiceFactory()
    : ProfileKeyedServiceFactory(
          "HttpEngagementKeyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

HttpsEngagementServiceFactory::~HttpsEngagementServiceFactory() = default;

std::unique_ptr<KeyedService>
HttpsEngagementServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HttpsEngagementService>();
}

bool HttpsEngagementServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
