// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/chromeos/cert_provisioning/mock_cert_provisioning_invalidator.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace chromeos {
namespace cert_provisioning {

// ================ MockCertProvisioningWorkerFactory ==========================

class MockCertProvisioningWorker;

class MockCertProvisioningWorkerFactory : public CertProvisioningWorkerFactory {
 public:
  MockCertProvisioningWorkerFactory();
  ~MockCertProvisioningWorkerFactory() override;

  MOCK_METHOD(std::unique_ptr<CertProvisioningWorker>,
              Create,
              (CertScope cert_scope,
               Profile* profile,
               PrefService* pref_service,
               const CertProfile& cert_profile,
               policy::CloudPolicyClient* cloud_policy_client,
               std::unique_ptr<CertProvisioningInvalidator> invalidator,
               base::RepeatingClosure state_change_callback,
               CertProvisioningWorkerCallback callback),
              (override));

  MOCK_METHOD(std::unique_ptr<CertProvisioningWorker>,
              Deserialize,
              (CertScope cert_scope,
               Profile* profile,
               PrefService* pref_service,
               const base::Value& saved_worker,
               policy::CloudPolicyClient* cloud_policy_client,
               std::unique_ptr<CertProvisioningInvalidator> invalidator,
               base::RepeatingClosure state_change_callback,
               CertProvisioningWorkerCallback callback),
              (override));

  MockCertProvisioningWorker* ExpectCreateReturnMock(
      CertScope cert_scope,
      const CertProfile& cert_profile);

  MockCertProvisioningWorker* ExpectDeserializeReturnMock(
      CertScope cert_scope,
      const base::Value& saved_worker);
};

// ================ MockCertProvisioningWorker =================================

class MockCertProvisioningWorker : public CertProvisioningWorker {
 public:
  MockCertProvisioningWorker();
  ~MockCertProvisioningWorker() override;

  MOCK_METHOD(void, DoStep, (), (override));
  MOCK_METHOD(void, Stop, (CertProvisioningWorkerState), (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(bool, IsWaiting, (), (const override));
  MOCK_METHOD(const CertProfile&, GetCertProfile, (), (const override));
  MOCK_METHOD(const std::string&, GetPublicKey, (), (const override));
  MOCK_METHOD(CertProvisioningWorkerState, GetState, (), (const override));
  MOCK_METHOD(CertProvisioningWorkerState,
              GetPreviousState,
              (),
              (const override));
  MOCK_METHOD(base::Time, GetLastUpdateTime, (), (const override));

  void SetExpectations(testing::Cardinality do_step_times,
                       bool is_waiting,
                       const CertProfile& cert_profile);

  // Stores |cert_profile| for SetExpectations function. It is returned by
  // reference and without copying it there is a risk that the original
  // CertProfile can be deleted before clearing the expectation.
  CertProfile cert_profile_;
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_
