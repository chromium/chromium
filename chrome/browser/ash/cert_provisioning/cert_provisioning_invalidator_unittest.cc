// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
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
        invalidation_handler_(
            CertProvisioningInvalidationHandler::BuildAndRegister(
                GetScope(),
                &invalidation_service_,
                kInvalidatorTopic,
                base::BindRepeating(&CertProvisioningInvalidationHandlerTest::
                                        OnIncomingInvalidation,
                                    base::Unretained(this)))) {
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
        invalidation::INVALIDATIONS_ENABLED);
  }

  void DisableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        invalidation::TRANSIENT_INVALIDATION_ERROR);
  }

  invalidation::Invalidation CreateInvalidation(
      const invalidation::Topic& topic) {
    return invalidation::Invalidation::InitUnknownVersion(topic);
  }

  invalidation::Invalidation FireInvalidation(
      const invalidation::Topic& topic) {
    const invalidation::Invalidation invalidation = CreateInvalidation(topic);
    invalidation_service_.EmitInvalidationForTest(invalidation);
    base::RunLoop().RunUntilIdle();
    return invalidation;
  }

  bool IsInvalidationSent(const invalidation::Invalidation& invalidation) {
    return !invalidation_service_.GetFakeAckHandler()->IsUnsent(invalidation);
  }

  bool IsInvalidationAcknowledged(
      const invalidation::Invalidation& invalidation) {
    return invalidation_service_.GetFakeAckHandler()->IsAcknowledged(
        invalidation);
  }

  bool IsInvalidatorRegistered(
      CertProvisioningInvalidationHandler* invalidator) const {
    return !invalidation_service_.invalidator_registrar()
                .GetRegisteredTopics(invalidator)
                .empty();
  }

  bool IsInvalidatorRegistered() const {
    return IsInvalidatorRegistered(invalidation_handler_.get());
  }

  void OnIncomingInvalidation() { ++incoming_invalidations_count_; }

  base::test::SingleThreadTaskEnvironment task_environment_;

  const invalidation::Topic kInvalidatorTopic;
  const invalidation::Topic kSomeOtherTopic;

  invalidation::FakeInvalidationService invalidation_service_;

  int incoming_invalidations_count_{0};

  std::unique_ptr<CertProvisioningInvalidationHandler> invalidation_handler_;
};

TEST_P(CertProvisioningInvalidationHandlerTest,
       SecondInvalidatorForSameTopicCannotBeBuilt) {
  EXPECT_NE(nullptr, invalidation_handler_);

  std::unique_ptr<CertProvisioningInvalidationHandler> second_invalidator =
      CertProvisioningInvalidationHandler::BuildAndRegister(
          GetScope(), &invalidation_service_, kInvalidatorTopic,
          base::DoNothing());

  EXPECT_EQ(nullptr, second_invalidator);
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ConstructorShouldNotRegisterInvalidator) {
  EXPECT_NE(nullptr, invalidation_handler_);

  CertProvisioningInvalidationHandler second_invalidator(
      GetScope(), &invalidation_service_, kSomeOtherTopic, base::DoNothing());

  EXPECT_FALSE(IsInvalidatorRegistered(&second_invalidator));
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ShouldReceiveInvalidationForRegisteredTopic) {
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_EQ(0, incoming_invalidations_count_);

  const auto invalidation = FireInvalidation(kInvalidatorTopic);

  EXPECT_TRUE(IsInvalidationSent(invalidation));
  EXPECT_TRUE(IsInvalidationAcknowledged(invalidation));
  EXPECT_EQ(1, incoming_invalidations_count_);
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ShouldNotReceiveInvalidationForDifferentTopic) {
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_EQ(0, incoming_invalidations_count_);

  const auto invalidation = FireInvalidation(kSomeOtherTopic);

  EXPECT_FALSE(IsInvalidationSent(invalidation));
  EXPECT_FALSE(IsInvalidationAcknowledged(invalidation));
  EXPECT_EQ(0, incoming_invalidations_count_);
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ShouldNotReceiveInvalidationWhenUnregistered) {
  EXPECT_TRUE(IsInvalidatorRegistered());

  invalidation_handler_->Unregister();

  EXPECT_FALSE(IsInvalidatorRegistered());

  const auto invalidation = FireInvalidation(kInvalidatorTopic);

  EXPECT_FALSE(IsInvalidationSent(invalidation));
  EXPECT_FALSE(IsInvalidationAcknowledged(invalidation));
  EXPECT_EQ(0, incoming_invalidations_count_);
}

TEST_P(CertProvisioningInvalidationHandlerTest,
       ShouldUnregisterButKeepTopicSubscribedWhenDestroyed) {
  EXPECT_TRUE(IsInvalidatorRegistered());

  invalidation_handler_.reset();

  // Ensure that invalidator is unregistered and incoming invalidation does not
  // cause undefined behaviour.
  EXPECT_FALSE(IsInvalidatorRegistered());
  FireInvalidation(kInvalidatorTopic);
  EXPECT_EQ(0, incoming_invalidations_count_);

  // Ensure that topic is still subscribed.
  const invalidation::Topics topics =
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
