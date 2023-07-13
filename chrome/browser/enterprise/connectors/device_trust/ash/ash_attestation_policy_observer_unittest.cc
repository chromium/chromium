// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/ash/ash_attestation_policy_observer.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class MockAshAttestationServiceWithWeakPtr : public AshAttestationService {
 public:
  ~MockAshAttestationServiceWithWeakPtr() override = default;

  MOCK_METHOD(void, TryPrepareKey, (), (override));

  MOCK_METHOD(void,
              BuildChallengeResponseForVAChallenge,
              (const std::string&,
               base::Value::Dict,
               const std::set<DTCPolicyLevel>&,
               AttestationCallback),
              (override));

  base::WeakPtr<MockAshAttestationServiceWithWeakPtr> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAshAttestationServiceWithWeakPtr> weak_factory_{
      this};
};

// Tests that only the enabling of an inline policy will trigger a key
// preparation.
TEST(AshAttestationPolicyObserverTest, TriggerKeyPreparation) {
  auto mock_permission_service = std::make_unique<
      testing::StrictMock<MockAshAttestationServiceWithWeakPtr>>();
  AshAttestationPolicyObserver observer(mock_permission_service->GetWeakPtr());

  // Disabling the policy should be no-op (won't trigger the StrictMock).
  observer.OnInlinePolicyDisabled(DTCPolicyLevel::kUser);
  observer.OnInlinePolicyDisabled(DTCPolicyLevel::kBrowser);

  EXPECT_CALL(*mock_permission_service, TryPrepareKey()).Times(2);
  observer.OnInlinePolicyEnabled(DTCPolicyLevel::kUser);
  observer.OnInlinePolicyEnabled(DTCPolicyLevel::kBrowser);

  // Make sure receiving a call after deleting the service doesn't cause any
  // issue.
  mock_permission_service.reset();
  observer.OnInlinePolicyEnabled(DTCPolicyLevel::kUser);
  observer.OnInlinePolicyEnabled(DTCPolicyLevel::kBrowser);
}

}  // namespace enterprise_connectors
