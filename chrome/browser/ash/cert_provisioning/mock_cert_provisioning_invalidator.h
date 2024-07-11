// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_INVALIDATOR_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_INVALIDATOR_H_

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace cert_provisioning {

class MockCertProvisioningInvalidatorFactory
    : public CertProvisioningInvalidatorFactory {
 public:
  MockCertProvisioningInvalidatorFactory();
  ~MockCertProvisioningInvalidatorFactory() override;

  MOCK_METHOD(std::unique_ptr<CertProvisioningInvalidator>,
              Create,
              (),
              (override));

  void ExpectCreateReturnNull();
};

class MockCertProvisioningInvalidator : public CertProvisioningInvalidator {
 public:
  MockCertProvisioningInvalidator();
  ~MockCertProvisioningInvalidator() override;

  MOCK_METHOD(void,
              Register,
              (const invalidation::Topic& topic,
               const std::string& listener_type,
               OnInvalidationEventCallback on_invalidation_event_callback),
              (override));

  MOCK_METHOD(void, Unregister, (), (override));
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_INVALIDATOR_H_
