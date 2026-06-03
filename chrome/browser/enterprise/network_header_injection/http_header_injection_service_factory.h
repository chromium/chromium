// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace enterprise_custom_headers {

class HttpHeaderInjectionService;

class HttpHeaderInjectionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static HttpHeaderInjectionService* GetForProfile(Profile* profile);
  static HttpHeaderInjectionServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<HttpHeaderInjectionServiceFactory>;

  HttpHeaderInjectionServiceFactory();
  ~HttpHeaderInjectionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace enterprise_custom_headers

#endif  // CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_SERVICE_FACTORY_H_
