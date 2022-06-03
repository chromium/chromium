// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_USER_SERVICE_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_USER_SERVICE_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {
namespace cert_provisioning {

class CertProvisioningSchedulerUserService : public KeyedService {
 public:
  explicit CertProvisioningSchedulerUserService(Profile* profile);
  ~CertProvisioningSchedulerUserService() override;

  CertProvisioningScheduler* scheduler() { return scheduler_.get(); }

 private:
  std::unique_ptr<CertProvisioningScheduler> scheduler_;
};

class CertProvisioningSchedulerUserServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static CertProvisioningSchedulerUserService* GetForProfile(Profile* profile);
  static CertProvisioningSchedulerUserServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<CertProvisioningSchedulerUserServiceFactory>;

  CertProvisioningSchedulerUserServiceFactory();
  ~CertProvisioningSchedulerUserServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  bool ServiceIsCreatedWithBrowserContext() const override;
  // BrowserStateKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace cert_provisioning
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when Chrome OS code migration is
// done.
namespace chromeos {
namespace cert_provisioning {
using ::ash::cert_provisioning::CertProvisioningSchedulerUserServiceFactory;
}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_USER_SERVICE_H_
