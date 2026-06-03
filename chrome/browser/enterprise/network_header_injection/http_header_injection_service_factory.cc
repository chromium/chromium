// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"

namespace enterprise_custom_headers {

// static
HttpHeaderInjectionService* HttpHeaderInjectionServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<HttpHeaderInjectionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
HttpHeaderInjectionServiceFactory*
HttpHeaderInjectionServiceFactory::GetInstance() {
  static base::NoDestructor<HttpHeaderInjectionServiceFactory> instance;
  return instance.get();
}

HttpHeaderInjectionServiceFactory::HttpHeaderInjectionServiceFactory()
    : ProfileKeyedServiceFactory("HttpHeaderInjectionServiceFactory",
                                 ProfileSelections::BuildForRegularProfile()) {}

HttpHeaderInjectionServiceFactory::~HttpHeaderInjectionServiceFactory() =
    default;

std::unique_ptr<KeyedService>
HttpHeaderInjectionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<HttpHeaderInjectionService>(profile->GetPrefs());
}

bool HttpHeaderInjectionServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace enterprise_custom_headers
