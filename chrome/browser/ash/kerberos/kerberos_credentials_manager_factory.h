// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ash {

class KerberosCredentialsManager;

// Singleton that creates and owns one KerberosCredentialsManager instance
// associated with primary profile. Note that KerberosCredentialsManager holds
// non-owning pointer to primary profile, so its life-time depends on the
// life-time of the primary profile.
class KerberosCredentialsManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Get existing service instance associated with the primary profile.
  // Note that the interface still expects the context in case primary profile
  // creation is not finalized. It returns nullptr if primary profile doesn't
  // exist or primary profile has changed.
  static KerberosCredentialsManager* GetExisting(
      content::BrowserContext* context);

  // Get existing service instance or create a service instance associated with
  // the primary profile.
  // Note that the interface still expects the context in case primary profile
  // creation is not finalized. It returns nullptr if primary profile doesn't
  // exist or primary profile has changed.
  static KerberosCredentialsManager* Get(content::BrowserContext* context);

  static KerberosCredentialsManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<KerberosCredentialsManagerFactory>;

  KerberosCredentialsManagerFactory();
  ~KerberosCredentialsManagerFactory() override;

  // Not copyable.
  KerberosCredentialsManagerFactory(const KerberosCredentialsManagerFactory&) =
      delete;
  KerberosCredentialsManagerFactory& operator=(
      const KerberosCredentialsManagerFactory&) = delete;

  bool ServiceIsCreatedWithBrowserContext() const override;

  // Returns nullptr in case context is not a primary profile. Otherwise returns
  // valid KerberosCredentialsManager.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // This is workaround to make sure we create only one service (singleton) and
  // prevent errors when two primary profiles are present (which normally
  // shouldn't happen, except in tests).
  // Additional reason to keep this workaround for now is that
  // KerberosCredentialsManager cannot be restarted at the moment, because it's
  // tightly coupled with KerberosClient singleton.
  // Note that it is potential risk for multi-threaded initialization (which is
  // not supported at the moment).
  mutable bool service_instance_created_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromOS code migration is done.
namespace chromeos {
using ::ash::KerberosCredentialsManagerFactory;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_FACTORY_H_
