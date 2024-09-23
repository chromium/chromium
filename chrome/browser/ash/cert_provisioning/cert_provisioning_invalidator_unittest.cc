// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace cert_provisioning {
namespace internal {

class CertProvisioningInvalidationHandlerTest
    : public testing::TestWithParam<CertScope> {
 protected:
  CertProvisioningInvalidationHandlerTest()
      : kInvalidatorTopic("abcdef"),
        kSomeOtherTopic("fedcba"),
        kListenerType("ABC123"),
        kSomeOtherType("321CBA"),
        invalidation_handler_(
            CertProvisioningInvalidationHandler::BuildAndRegister(
                GetScope(),
                &invalidation_service_,
                kInvalidatorTopic,
                kListenerType,
                invalidation_events_.GetRepeatingCallback())) {
    EXPECT_NE(nullptr, invalidation_handler_);

    EnableInvalidationService();
  }

  CertProvisioningInvalidationHandlerTest(
      const CertProvisioningInvalidationHandlerTest&) = delete;
  CertProvisioningInvalidationHandlerTest& operator=(
      const CertProvisioningInvalidationHandlerTest&) = delete;

  CertScope GetScope() const { return GetParam(); }

  const char* GetExpectedOwnerName() const {
    switch (GetScope()) {
      case CertScope::kUser:
        return "cert.user.abcdef";
      case CertScope::kDevice:
        return "cert.device.abcdef";
    }
  }

  void EnableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        invalidation::InvalidatorState::kEnabled);
  }

  void DisableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        invalidation::InvalidatorState::kDisabled);
  }

  invalidation::Invalidation CreateInvalidation(
      const invalidation::Topic& topic) {
    return invalidation::Invalidation(topic, 42, "foo");
  }

  // Will send invalidation to handler if `IsInvalidatorRegistered(topic) ==
  // true`.
  invalidation::Invalidation FireInvalidation(
      const invalidation::Topic& topic) {
    const invalidation::Invalidation invalidation = CreateInvalidation(topic);
    invalidation_service_.EmitInvalidationForTest(invalidation);
    return invalidation;
  }

  bool IsInvalidatorRegistered(
      CertProvisioningInvalidationHandler* invalidator) const {
    return invalidation_service_.HasObserver(invalidator);
  }

  bool IsInvalidatorRegistered(const invalidation::Topic& topic) {
    return invalidation_service_.invalidator_registrar()
        .GetRegisteredTopics(invalidation_handler_.get())
        .contains(topic);
  }

  bool IsInvalidatorRegistered() const {
    return IsInvalidatorRegistered(invalidation_handler_.get());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  const invalidation::Topic kInvalidatorTopic;
  const invalidation::Topic kSomeOtherTopic;
  const std::string kListenerType;
  const std::string kSomeOtherType;

  invalidation::FakeInvalidationService invalidation_service_;

  base::test::TestFuture<InvalidationEvent> invalidation_events_;

  std::unique_ptr<CertProvisioningInvalidationHandler> invalidation_handler_;
};

TEST_P(CertProvisioningInvalidationHandlerTest,
       SecondInvalidatorForSameTopicCannotBeBuilt) {
  EXPECT_NE(nullptr, invalidation_handler_);

  std::unique_ptr<CertProvisioningInvalidationHandler> second_invalidator =
      CertProvisioningInvalidationHandler::BuildAndRegister(
          GetScope(), &invalidation_service_, kInvalidatorTopic, kListenerType,
          base::DoNothing());

  EXPECT_EQ(nullptr, second_invalidator);
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ConstructorShouldNotRegisterInvalidator) {
  EXPECT_NE(nullptr, invalidation_handler_);

  CertProvisioningInvalidationHandler second_invalidator(
      GetScope(), &invalidation_service_, kSomeOtherTopic, kSomeOtherType,
      base::DoNothing());

  EXPECT_FALSE(IsInvalidatorRegistered(&second_invalidator));
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ShouldReceiveInvalidationForRegisteredTopic) {
  EXPECT_TRUE(IsInvalidatorRegistered(kInvalidatorTopic));

  FireInvalidation(kInvalidatorTopic);

  EXPECT_EQ(invalidation_events_.Take(),
            InvalidationEvent::kInvalidationReceived);
}

TEST_P(CertProvisioningInvalidationHandlerTest, ShouldUnregister) {
  EXPECT_TRUE(IsInvalidatorRegistered(kInvalidatorTopic));

  invalidation_handler_->Unregister();

  EXPECT_FALSE(IsInvalidatorRegistered());
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ShouldUnregisterButKeepTopicSubscribedWhenDestroyed) {
  EXPECT_TRUE(IsInvalidatorRegistered());

  invalidation_handler_.reset();

  // Ensure that invalidator is unregistered.
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_FALSE(invalidation_events_.IsReady());

  // Ensure that topic is still subscribed.
  const invalidation::TopicMap topics =
      invalidation_service_.invalidator_registrar().GetAllSubscribedTopics();
  EXPECT_NE(topics.end(), topics.find(kInvalidatorTopic));
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ShouldHaveUniqueOwnerNameContainingScopeAndTopic) {
  EXPECT_EQ(GetExpectedOwnerName(), invalidation_handler_->GetOwnerName());
}

INSTANTIATE_TEST_SUITE_P(CertProvisioningInvalidationHandlerTestInstance,
                         CertProvisioningInvalidationHandlerTest,
                         testing::Values(CertScope::kUser, CertScope::kDevice));

}  // namespace internal
}  // namespace cert_provisioning
}  // namespace ash
