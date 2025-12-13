// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cert_provisioning::internal {

namespace {

constexpr char kListenerType[] = "ABC123";
constexpr char kSomeOtherType[] = "321CBA";

}  // namespace

class CertProvisioningInvalidationHandlerTest : public testing::Test {
 protected:
  CertProvisioningInvalidationHandlerTest()
      : invalidation_handler_(
            std::make_unique<CertProvisioningInvalidationHandler>(
                &invalidation_listener_,
                kListenerType,
                invalidation_events_.GetRepeatingCallback())) {
    EXPECT_NE(nullptr, invalidation_handler_);

    invalidation_listener_.Start();
  }

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

TEST_F(CertProvisioningInvalidationHandlerTest,
       ConstructorRegistersInvalidator) {
  EXPECT_NE(nullptr, invalidation_handler_);

  CertProvisioningInvalidationHandler second_invalidator(
      &invalidation_listener_, kSomeOtherType, base::DoNothing());

  EXPECT_TRUE(IsInvalidatorRegistered(&second_invalidator));
}

TEST_F(CertProvisioningInvalidationHandlerTest,
       ShouldReportSubscriptionWhenInvalidationListenerRestarts) {
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_EQ(invalidation_events_.Take(),
            InvalidationEvent::kSuccessfullySubscribed);

  invalidation_listener_.Shutdown();
  invalidation_listener_.Start();
  EXPECT_EQ(invalidation_events_.Take(),
            InvalidationEvent::kSuccessfullySubscribed);
}

TEST_F(CertProvisioningInvalidationHandlerTest,
       ShouldReceiveInvalidationForType) {
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_EQ(invalidation_events_.Take(),
            InvalidationEvent::kSuccessfullySubscribed);

  FireInvalidation(kListenerType);
  EXPECT_EQ(invalidation_events_.Take(),
            InvalidationEvent::kInvalidationReceived);
}

TEST_F(CertProvisioningInvalidationHandlerTest, ShouldHaveCorrectTypeName) {
  EXPECT_EQ(kListenerType, invalidation_handler_->GetType());
}

}  // namespace ash::cert_provisioning::internal
