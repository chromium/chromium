// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_FACTORY_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class PrefRegistrySimple;
class Profile;

namespace policy {

class PolicyCertService;

// Factory to create PolicyCertServices.
class PolicyCertServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an existing PolicyCertService for |profile|. See
  // CreateForProfile.
  static PolicyCertService* GetForProfile(Profile* profile);

  // Creates (if it's not already created) a PolicyCertService and gets it to
  // start listening for trust anchors for the profile. Returns false if this
  // service isn't allowed for |profile|, i.e. if NetworkConfigurationUpdater
  // doesn't exist. This service is created separately for the original profile
  // and the incognito profile.
  // Note: NetworkConfigurationUpdater is currently only created for the primary
  // user's profile.
  // This should only be called if the network service is enabled.
  static bool CreateAndStartObservingForProfile(Profile* profile);

  static PolicyCertServiceFactory* GetInstance();

  // Used to mark or clear |user_id| as having used certificates pushed by
  // policy before.
  static void SetUsedPolicyCertificates(const std::string& user_id);
  static void ClearUsedPolicyCertificates(const std::string& user_id);
  static bool UsedPolicyCertificates(const std::string& user_id);

  static void RegisterPrefs(PrefRegistrySimple* local_state);

 private:
  friend struct base::DefaultSingletonTraits<PolicyCertServiceFactory>;

  PolicyCertServiceFactory();
  ~PolicyCertServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(PolicyCertServiceFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_FACTORY_H_
