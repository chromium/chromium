// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cert_provisioning::internal {

namespace {

constexpr char kLegacyInvalidationTopic[] = "abcd";

constexpr char kListenerType[] = "ABC123";
constexpr char kSomeOtherType[] = "321CBA";

}  // namespace

class CertProvisioningInvalidationHandlerWithInvalidationListenerTest
    : public testing::TestWithParam<CertScope> {
 protected:
  CertProvisioningInvalidationHandlerWithInvalidationListenerTest()
      : invalidation_handler_(
            CertProvisioningInvalidationHandler::BuildAndRegister(
                GetScope(),
                &invalidation_listener_,
                kLegacyInvalidationTopic,
                kListenerType,
                invalidation_events_.GetRepeatingCallback())) {
    EXPECT_NE(nullptr, invalidation_handler_);

    invalidation_listener_.Start();
  }

  CertScope GetScope() const { return GetParam(); }

  invalidation::DirectInvalidation CreateInvalidation(std::string type) {
    return invalidation::DirectInvalidation(std::move(type), /*version=*/42,
                                            /*payload=*/"foo");
  }

  // Will send invalidation to handler if `invalidator_` is registered and
  // `invalidation_listener_` started.
  invalidation::DirectInvalidation FireInvalidation(std::string type) {
    const invalidation::DirectInvalidation invalidation =
        CreateInvalidation(std::move(type));
    invalidation_listener_.FireInvalidation(invalidation);
    return invalidation;
  }

  bool IsInvalidatorRegistered(
      CertProvisioningInvalidationHandler* invalidator) const {
    return invalidation_listener_.HasObserver(invalidator);
  }

  bool IsInvalidatorRegistered() const {
    return IsInvalidatorRegistered(invalidation_handler_.get());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  invalidation::FakeInvalidationListener invalidation_listener_;

  base::test::TestFuture<InvalidationEvent> invalidation_events_;

  std::unique_ptr<CertProvisioningInvalidationHandler> invalidation_handler_;
};

TEST_P(CertProvisioningInvalidationHandlerWithInvalidationListenerTest,
       SecondInvalidatorForSameTopicCanBeBuilt) {
  EXPECT_NE(nullptr, invalidation_handler_);

  std::unique_ptr<CertProvisioningInvalidationHandler> second_invalidator =
      CertProvisioningInvalidationHandler::BuildAndRegister(
          GetScope(), &invalidation_listener_, kLegacyInvalidationTopic,
          kSomeOtherType, base::DoNothing());

  EXPECT_NE(nullptr, second_invalidator);
}

TEST_P(CertProvisioningInvalidationHandlerWithInvalidationListenerTest,
       ConstructorShouldNotRegisterInvalidator) {
  EXPECT_NE(nullptr, invalidation_handler_);

  CertProvisioningInvalidationHandler second_invalidator(
      GetScope(), &invalidation_listener_, kLegacyInvalidationTopic,
      kSomeOtherType, base::DoNothing());

  EXPECT_FALSE(IsInvalidatorRegistered(&second_invalidator));
}

TEST_P(CertProvisioningInvalidationHandlerWithInvalidationListenerTest,
       ShouldReceiveInvalidationForType) {
  EXPECT_TRUE(IsInvalidatorRegistered());

  FireInvalidation(kListenerType);

  EXPECT_EQ(invalidation_events_.Take(),
            InvalidationEvent::kInvalidationReceived);
}

TEST_P(CertProvisioningInvalidationHandlerWithInvalidationListenerTest,
       ShouldUnregister) {
  EXPECT_TRUE(IsInvalidatorRegistered());

  invalidation_handler_->Unregister();

  EXPECT_FALSE(IsInvalidatorRegistered());
}

TEST_P(CertProvisioningInvalidationHandlerWithInvalidationListenerTest,
       ShouldHaveCorrectTypeName) {
  EXPECT_EQ(kListenerType, invalidation_handler_->GetType());
}

INSTANTIATE_TEST_SUITE_P(
    CertProvisioningInvalidationHandlerWithInvalidationListenerTestInstance,
    CertProvisioningInvalidationHandlerWithInvalidationListenerTest,
    testing::Values(CertScope::kUser, CertScope::kDevice));

}  // namespace ash::cert_provisioning::internal
