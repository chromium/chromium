// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/dm_token_retriever_provider.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/messaging_layer/util/user_dm_token_retriever.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kDMToken[] = "TOKEN";
constexpr char kUserId[] = "test-user";

class DMTokenRetrieverProviderTest : public ::testing::Test {
 protected:
  DMTokenRetrieverProviderTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    // Ensure test profile manager is set up
    CHECK(testing_profile_manager_.SetUp())
        << "TestingProfileManager not setup";

    // Create test profile so profile manager does not crash when trying to
    // fetch an active user profile during DM token retrieval
    testing_profile_manager_.CreateTestingProfile(kUserId);
  }

  void SetUp() override {
    dm_token_retriever_provider_ = std::make_unique<DMTokenRetrieverProvider>();
  }

  void TearDown() override { dm_token_retriever_provider_.reset(); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<DMTokenRetrieverProvider> dm_token_retriever_provider_;
};

TEST_F(DMTokenRetrieverProviderTest,
       ReturnsEmptyDMTokenRetrieverForNonUserEvents) {
  auto dm_token_retriever =
      std::move(dm_token_retriever_provider_)
          ->GetDMTokenRetrieverForEventType(EventType::kDevice);
  ASSERT_THAT(dm_token_retriever.get(), Ne(nullptr));

  // Verify it is a EmptyDMTokenRetriever by attempting to retrieve the DM token
  // and validating it is empty.
  test::TestEvent<StatusOr<std::string>> dm_token_retrieved_event;
  dm_token_retriever->RetrieveDMToken(dm_token_retrieved_event.cb());
  auto dm_token_result = dm_token_retrieved_event.result();
  ASSERT_OK(dm_token_result);
  EXPECT_THAT(dm_token_result.value(), IsEmpty());
}

TEST_F(DMTokenRetrieverProviderTest, ReturnsUserDMTokenRetrieverForUserEvents) {
  auto dm_token_retriever =
      std::move(dm_token_retriever_provider_)
          ->GetDMTokenRetrieverForEventType(EventType::kUser);
  ASSERT_THAT(dm_token_retriever.get(), Ne(nullptr));

  // Verify it is a UserDMTokenRetriever by mocking profile based DM token
  // retrieval to return the test DM token and comparing the result of the
  // retrieval with this token.
  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDMToken));
  test::TestEvent<StatusOr<std::string>> dm_token_retrieved_event;
  dm_token_retriever->RetrieveDMToken(dm_token_retrieved_event.cb());
  auto dm_token_result = dm_token_retrieved_event.result();
  ASSERT_OK(dm_token_result);
  EXPECT_THAT(dm_token_result.value(), StrEq(kDMToken));
}

TEST_F(DMTokenRetrieverProviderTest,
       ReturnsUniqueEmptyDMTokenRetrieversWhenMultipleCalls) {
  auto dm_token_retriever_1 =
      dm_token_retriever_provider_->GetDMTokenRetrieverForEventType(
          EventType::kDevice);
  ASSERT_THAT(dm_token_retriever_1.get(), Ne(nullptr));

  auto dm_token_retriever_2 =
      std::move(dm_token_retriever_provider_)
          ->GetDMTokenRetrieverForEventType(EventType::kDevice);
  ASSERT_THAT(dm_token_retriever_2.get(), Ne(nullptr));

  EXPECT_NE(dm_token_retriever_1.get(), dm_token_retriever_2.get());
}

TEST_F(DMTokenRetrieverProviderTest,
       ReturnsUniqueUserDMTokenRetrieversWhenMultipleCalls) {
  auto dm_token_retriever_1 =
      dm_token_retriever_provider_->GetDMTokenRetrieverForEventType(
          EventType::kUser);
  ASSERT_THAT(dm_token_retriever_1.get(), Ne(nullptr));

  auto dm_token_retriever_2 =
      std::move(dm_token_retriever_provider_)
          ->GetDMTokenRetrieverForEventType(EventType::kUser);
  ASSERT_THAT(dm_token_retriever_2.get(), Ne(nullptr));

  EXPECT_NE(dm_token_retriever_1.get(), dm_token_retriever_2.get());
}

}  // namespace
}  // namespace reporting
