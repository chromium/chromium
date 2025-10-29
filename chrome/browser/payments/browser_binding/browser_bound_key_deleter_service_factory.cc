// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/browser_context.h"

namespace payments {

// static
BrowserBoundKeyDeleterService*
BrowserBoundKeyDeleterServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BrowserBoundKeyDeleterService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
BrowserBoundKeyDeleterServiceFactory*
BrowserBoundKeyDeleterServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserBoundKeyDeleterServiceFactory> instance;
  return instance.get();
}

void BrowserBoundKeyDeleterServiceFactory::SetServiceForTesting(
    std::unique_ptr<BrowserBoundKeyDeleterService> service) {
  service_for_testing_ = std::move(service);
}

BrowserBoundKeyDeleterServiceFactory::BrowserBoundKeyDeleterServiceFactory()
    : ProfileKeyedServiceFactory(
          "BrowserBoundKeyDeleterService",
          // Browser bound key should not be deleted in off the record profiles
          // as they are not created in them either.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(webdata_services::WebDataServiceWrapperFactory::GetInstance());
}

BrowserBoundKeyDeleterServiceFactory::~BrowserBoundKeyDeleterServiceFactory() =
    default;

std::unique_ptr<KeyedService>
BrowserBoundKeyDeleterServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<BrowserBoundKeyDeleterService> service;
  if (service_for_testing_) {
    service = std::move(service_for_testing_);
  } else {
    service = GetBrowserBoundKeyDeleterServiceInstance(
        webdata_services::WebDataServiceWrapperFactory::
            GetWebPaymentsWebDataServiceForBrowserContext(
                context, ServiceAccessType::EXPLICIT_ACCESS));
  }

  // This triggers a cleanup of browser bound keys at startup (and the service
  // may be used later for explicit cleanup from delete browsing data).
  service->RemoveInvalidBBKs();

  return service;
}

bool BrowserBoundKeyDeleterServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace payments
