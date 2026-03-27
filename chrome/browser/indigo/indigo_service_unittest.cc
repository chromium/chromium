// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_service.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace indigo {

class IndigoServiceTest : public testing::Test {
 public:
  void CreateService() {
    service_ = std::make_unique<IndigoService>(
        &profile_, identity_test_env_.identity_manager());
  }

  void MakeAccountAvailableAndCapable() {
    AccountInfo info = identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_.UpdateAccountInfoForAccount(info);
  }

  ::testing::AssertionResult LocalEligibilityBecomes(
      ::testing::Matcher<LocalEligibility> matcher) {
    if (matcher.Matches(service_->GetLocalEligibility())) {
      return ::testing::AssertionSuccess();
    }
    base::test::TestFuture<LocalEligibility> future{
        base::test::TestFutureMode::kQueue};
    auto sub = service_->RegisterLocalEligibilityChangedCallback(
        future.GetRepeatingCallback());
    while (future.Wait()) {
      LocalEligibility eligibility = future.Take();
      if (eligibility != service_->GetLocalEligibility()) {
        return ::testing::AssertionFailure()
               << "notification doesn't match the current eligibility";
      }
      if (matcher.Matches(eligibility)) {
        return ::testing::AssertionSuccess();
      }
    }
    return ::testing::AssertionFailure() << "timed out";
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<IndigoService> service_;
};

TEST_F(IndigoServiceTest, DefaultStateNotSignedIn) {
  CreateService();
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kNotSignedIn);
}

TEST_F(IndigoServiceTest, SignIn) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));
}

TEST_F(IndigoServiceTest, CapabilitiesDisable) {
  CreateService();

  AccountInfo info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&info.capabilities);
  mutator.set_can_use_model_execution_features(false);
  identity_test_env_.UpdateAccountInfoForAccount(info);

  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kMissingCapabilities));
}

}  // namespace indigo
