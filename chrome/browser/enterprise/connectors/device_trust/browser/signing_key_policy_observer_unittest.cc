// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/browser/siging_key_policy_observer.h"

#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_device_trust_key_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using test::MockDeviceTrustKeyManager;

class SigningKeyPolicyObserverTest : public testing::Test {
 protected:
  SigningKeyPolicyObserverTest() : observer_(&mock_key_manager_) {}

  testing::StrictMock<MockDeviceTrustKeyManager> mock_key_manager_;
  SigningKeyPolicyObserver observer_;
};

TEST_F(SigningKeyPolicyObserverTest, InlinePolicyEnabled) {
  // Policy enabled at user-level, nothing happens.
  observer_.OnInlinePolicyEnabled(DTCPolicyLevel::kUser);

  // Policy enabled at browser-level, browser key is initialized.
  EXPECT_CALL(mock_key_manager_, StartInitialization());
  observer_.OnInlinePolicyEnabled(DTCPolicyLevel::kBrowser);
}

TEST_F(SigningKeyPolicyObserverTest, InlinePolicyDisabled) {
  // Policy disabled at either level, nothing happens.
  observer_.OnInlinePolicyDisabled(DTCPolicyLevel::kUser);
  observer_.OnInlinePolicyDisabled(DTCPolicyLevel::kBrowser);
}

}  // namespace enterprise_connectors
