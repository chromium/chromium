// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORKING_POLICY_CERT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_NETWORKING_POLICY_CERT_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class Profile;

namespace policy {

class PolicyCertService;

// Factory to create PolicyCertServices.
class PolicyCertServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns a PolicyCertService for `profile`. This service is created
  // separately for the original profile and the incognito profile.
  // Note: NetworkConfigurationUpdater is currently only created for the primary
  // user's profile.
  static PolicyCertService* GetForProfile(Profile* profile);

  static PolicyCertServiceFactory* GetInstance();

  PolicyCertServiceFactory(const PolicyCertServiceFactory&) = delete;
  PolicyCertServiceFactory& operator=(const PolicyCertServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PolicyCertServiceFactory>;

  PolicyCertServiceFactory();
  ~PolicyCertServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORKING_POLICY_CERT_SERVICE_FACTORY_H_
