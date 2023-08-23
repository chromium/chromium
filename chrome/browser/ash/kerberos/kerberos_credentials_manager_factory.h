// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ash {

class KerberosCredentialsManager;

// Singleton that creates and owns one KerberosCredentialsManager instance
// associated with each primary profile. Note that each
// KerberosCredentialsManager holds a non-owned pointer to its respective
// primary profile, so its life-time depends on the life-time of that profile.
// Multiple primary profiles only happen in tests.
class KerberosCredentialsManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the existing service instance associated with the given profile.
  // Returns nullptr for non-primary profiles.
  static KerberosCredentialsManager* GetExisting(
      content::BrowserContext* context);

  // Gets the existing service instance or creates a service instance associated
  // with the given profile. Returns nullptr for non-primary profiles.
  static KerberosCredentialsManager* Get(content::BrowserContext* context);

  // Gets a singleton instance of the factory.
  static KerberosCredentialsManagerFactory* GetInstance();

  // Disallow copy and assignment.
  KerberosCredentialsManagerFactory(const KerberosCredentialsManagerFactory&) =
      delete;
  KerberosCredentialsManagerFactory& operator=(
      const KerberosCredentialsManagerFactory&) = delete;

 private:
  friend base::NoDestructor<KerberosCredentialsManagerFactory>;

  KerberosCredentialsManagerFactory();
  ~KerberosCredentialsManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides: -----------------------------
  bool ServiceIsCreatedWithBrowserContext() const override;

  // Returns nullptr in case context is not a primary profile. Otherwise returns
  // a valid KerberosCredentialsManager.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_FACTORY_H_
