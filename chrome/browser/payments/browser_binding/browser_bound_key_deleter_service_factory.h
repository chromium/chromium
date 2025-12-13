// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {

class BrowserContext;

}  // namespace content

namespace payments {

class BrowserBoundKeyDeleterService;

// Responsible for creating a service to start the process of finding and
// deleting invalid browser bound key metadata metadata.
class BrowserBoundKeyDeleterServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BrowserBoundKeyDeleterService* GetForProfile(Profile* profile);
  static BrowserBoundKeyDeleterServiceFactory* GetInstance();

  // This method must be called before the service is constructed in
  // BuildServiceInstanceForBrowserContext() to have an effect.
  void SetServiceForTesting(
      std::unique_ptr<BrowserBoundKeyDeleterService> service);

 private:
  friend base::NoDestructor<BrowserBoundKeyDeleterServiceFactory>;

  BrowserBoundKeyDeleterServiceFactory();
  ~BrowserBoundKeyDeleterServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  // In order to return this testing service from the const method
  // BuildServiceInstanceForBrowserContext(), the member needs to be mutable.
  mutable std::unique_ptr<BrowserBoundKeyDeleterService> service_for_testing_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_FACTORY_H_
