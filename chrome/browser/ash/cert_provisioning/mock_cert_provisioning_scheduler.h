// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_SCHEDULER_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_SCHEDULER_H_

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace cert_provisioning {

class MockCertProvisioningScheduler : public CertProvisioningScheduler {
 public:
  MockCertProvisioningScheduler();
  MockCertProvisioningScheduler(const MockCertProvisioningScheduler&) = delete;
  MockCertProvisioningScheduler& operator=(
      const MockCertProvisioningScheduler&) = delete;
  ~MockCertProvisioningScheduler() override;

  MOCK_METHOD(bool,
              UpdateOneWorker,
              (const CertProfileId& cert_profile_id),
              (override));
  MOCK_METHOD(bool,
              ResetOneWorker,
              (const CertProfileId& cert_profile_id),
              (override));
  MOCK_METHOD(void, UpdateAllWorkers, (), (override));
  MOCK_METHOD(const WorkerMap&, GetWorkers, (), (const override));
  MOCK_METHOD((const base::flat_map<CertProfileId, FailedWorkerInfo>&),
              GetFailedCertProfileIds,
              (),
              (const override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddObserver,
              (base::RepeatingClosure callback),
              (override));
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_SCHEDULER_H_
