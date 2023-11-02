// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_invalidator.h"

namespace ash {
namespace cert_provisioning {

//============= MockCertProvisioningInvalidatorFactory =========================

std::unique_ptr<CertProvisioningInvalidator> FakeCreate() {
  return nullptr;
}

MockCertProvisioningInvalidatorFactory::
    MockCertProvisioningInvalidatorFactory() = default;
MockCertProvisioningInvalidatorFactory::
    ~MockCertProvisioningInvalidatorFactory() = default;

void MockCertProvisioningInvalidatorFactory::ExpectCreateReturnNull() {
  EXPECT_CALL(*this, Create).WillRepeatedly(testing::Invoke(FakeCreate));
}

//============= MockCertProvisioningInvalidator ================================

MockCertProvisioningInvalidator::MockCertProvisioningInvalidator() = default;
MockCertProvisioningInvalidator::~MockCertProvisioningInvalidator() = default;

}  // namespace cert_provisioning
}  // namespace ash
