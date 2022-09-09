// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORKING_POLICY_CERT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_NETWORKING_POLICY_CERT_SERVICE_FACTORY_H_

#include <memory>
#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class Profile;

namespace policy {

class PolicyCertService;

// Factory to create PolicyCertServices.
class PolicyCertServiceFactory : public ProfileKeyedServiceFactory {
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

  PolicyCertServiceFactory(const PolicyCertServiceFactory&) = delete;
  PolicyCertServiceFactory& operator=(const PolicyCertServiceFactory&) = delete;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Migrates the `prefs::kUsedPolicyCertificates` preference from local state
  // into per-profile pref storage. Returns true if the local pref was present
  // and was successfully migrated.
  // TODO(b/202492163): The migration started in October 2021. According to
  // chrome/browser/prefs/README.md it should go for at least a year. Also note
  // that Lacros-Chrome can never have this preference in local state.
  static bool MigrateLocalStatePrefIntoProfilePref(
      const std::string& user_email,
      Profile* profile);
  // Used to clear |user_email| as having used certificates pushed by
  // policy before. Returns true if the flag was present and was successfully
  // cleared.
  // TODO(b/202492163) This can be deleted after the local state -> profile pref
  // migration is finished.
  static bool ClearUsedPolicyCertificates(const std::string& user_email);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  friend struct base::DefaultSingletonTraits<PolicyCertServiceFactory>;

  PolicyCertServiceFactory();
  ~PolicyCertServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORKING_POLICY_CERT_SERVICE_FACTORY_H_
