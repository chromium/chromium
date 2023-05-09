// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/consent_policy_observer.h"

#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/mock_user_permission_service.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class MockUserPermissionServiceWithWeakPtr
    : public device_signals::MockUserPermissionService {
 public:
  ~MockUserPermissionServiceWithWeakPtr() override = default;

  base::WeakPtr<MockUserPermissionServiceWithWeakPtr> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockUserPermissionServiceWithWeakPtr> weak_factory_{
      this};
};

// Tests that only disabling of the user-level policy can trigger a consent
// reset check.
TEST(ConsentPolicyObserverTest, TriggerConsentReset) {
  auto mock_permission_service = std::make_unique<
      testing::StrictMock<MockUserPermissionServiceWithWeakPtr>>();
  ConsentPolicyObserver observer(mock_permission_service->GetWeakPtr());

  // All of the following should be no-op (won't trigger the StrictMock).
  observer.OnInlinePolicyEnabled(DTCPolicyLevel::kUser);
  observer.OnInlinePolicyEnabled(DTCPolicyLevel::kBrowser);
  observer.OnInlinePolicyDisabled(DTCPolicyLevel::kBrowser);

  EXPECT_CALL(*mock_permission_service, ResetUserConsentIfNeeded());
  observer.OnInlinePolicyDisabled(DTCPolicyLevel::kUser);

  // Make sure receiving a call after deleting the service doesn't cause any
  // issue.
  mock_permission_service.reset();
  observer.OnInlinePolicyDisabled(DTCPolicyLevel::kUser);
}

}  // namespace enterprise_connectors
