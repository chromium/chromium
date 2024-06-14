// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_invalidator.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace ash {
namespace cert_provisioning {

// ================ MockCertProvisioningWorkerFactory ==========================

class MockCertProvisioningWorker;

class MockCertProvisioningWorkerFactory : public CertProvisioningWorkerFactory {
 public:
  MockCertProvisioningWorkerFactory();
  ~MockCertProvisioningWorkerFactory() override;

  MOCK_METHOD(std::unique_ptr<CertProvisioningWorker>,
              Create,
              (std::string process_id,
               CertScope cert_scope,
               Profile* profile,
               PrefService* pref_service,
               const CertProfile& cert_profile,
               CertProvisioningClient* cert_provisioning_client,
               std::unique_ptr<CertProvisioningInvalidator> invalidator,
               base::RepeatingClosure state_change_callback,
               CertProvisioningWorkerCallback callback),
              (override));

  MOCK_METHOD(std::unique_ptr<CertProvisioningWorker>,
              Deserialize,
              (CertScope cert_scope,
               Profile* profile,
               PrefService* pref_service,
               const base::Value::Dict& saved_worker,
               CertProvisioningClient* cert_provisioning_client,
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
  MOCK_METHOD(void, MarkWorkerForReset, (), (override));
  MOCK_METHOD(bool, IsWaiting, (), (const override));
  MOCK_METHOD(bool, IsWorkerMarkedForReset, (), (const override));
  MOCK_METHOD(const std::optional<BackendServerError>&,
              GetLastBackendServerError,
              (),
              (const override));
  MOCK_METHOD(const CertProfile&, GetCertProfile, (), (const override));
  MOCK_METHOD(const std::vector<uint8_t>&, GetPublicKey, (), (const override));
  MOCK_METHOD(CertProvisioningWorkerState, GetState, (), (const override));
  MOCK_METHOD(CertProvisioningWorkerState,
              GetPreviousState,
              (),
              (const override));
  MOCK_METHOD(base::Time, GetLastUpdateTime, (), (const override));
  MOCK_METHOD(std::string, GetFailureMessage, (), (const override));

  void SetExpectations(testing::Cardinality do_step_times,
                       bool is_waiting,
                       const CertProfile& cert_profile,
                       std::string failure_message);
  void ResetExpected();

  // Storage fields for SetExpectations function. They are returned by
  // reference and without copying them there is a risk that the original
  // objects can be deleted before clearing the expectation.
  CertProfile cert_profile_;
  std::string failure_message_;
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_WORKER_H_
