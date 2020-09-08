// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/mock_cert_provisioning_worker.h"
#include "base/test/values_test_util.h"

using base::test::IsJson;
using testing::_;
using testing::Return;
using testing::ReturnRef;

namespace chromeos {
namespace cert_provisioning {

// ================ MockCertProvisioningWorkerFactory ==========================

MockCertProvisioningWorkerFactory::MockCertProvisioningWorkerFactory() =
    default;

MockCertProvisioningWorkerFactory::~MockCertProvisioningWorkerFactory() =
    default;

MockCertProvisioningWorker*
MockCertProvisioningWorkerFactory::ExpectCreateReturnMock(
    CertScope cert_scope,
    const CertProfile& cert_profile) {
  auto mock_worker = std::make_unique<MockCertProvisioningWorker>();
  MockCertProvisioningWorker* pointer = mock_worker.get();

  EXPECT_CALL(*this, Create(cert_scope, _, _, cert_profile, _, _, _, _))
      .Times(1)
      .WillOnce(Return(testing::ByMove(std::move(mock_worker))));

  return pointer;
}

MockCertProvisioningWorker*
MockCertProvisioningWorkerFactory::ExpectDeserializeReturnMock(
    CertScope cert_scope,
    const base::Value& saved_worker) {
  auto mock_worker = std::make_unique<MockCertProvisioningWorker>();
  MockCertProvisioningWorker* pointer = mock_worker.get();

  EXPECT_CALL(*this,
              Deserialize(cert_scope, _, _, IsJson(saved_worker), _, _, _, _))
      .Times(1)
      .WillOnce(Return(testing::ByMove(std::move(mock_worker))));

  return pointer;
}

// ================ MockCertProvisioningWorker =================================

MockCertProvisioningWorker::MockCertProvisioningWorker() {
  static const std::string default_public_key;
  ON_CALL(*this, GetPublicKey).WillByDefault(ReturnRef(default_public_key));
}

MockCertProvisioningWorker::~MockCertProvisioningWorker() = default;

void MockCertProvisioningWorker::SetExpectations(
    testing::Cardinality do_step_times,
    bool is_waiting,
    const CertProfile& cert_profile) {
  testing::Mock::VerifyAndClearExpectations(this);

  cert_profile_ = cert_profile;

  EXPECT_CALL(*this, DoStep).Times(do_step_times);
  EXPECT_CALL(*this, IsWaiting).WillRepeatedly(Return(is_waiting));
  EXPECT_CALL(*this, GetCertProfile).WillRepeatedly(ReturnRef(cert_profile_));
}

}  // namespace cert_provisioning
}  // namespace chromeos
