// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/chrome_serialized_navigation_driver.h"

#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "ui/base/page_transition_types.h"

class ChromeSerializedNavigationDriverTest : public ::testing::Test {
 public:
  ChromeSerializedNavigationDriverTest() {}
  ~ChromeSerializedNavigationDriverTest() override {}

  void SetUp() override {
    sessions::ContentSerializedNavigationDriver::SetInstance(
        ChromeSerializedNavigationDriver::GetInstance());
  }

  void TearDown() override {
    sessions::ContentSerializedNavigationDriver::SetInstance(nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSerializedNavigationDriverTest);
};

// Tests that the input data is left unsanitized when the referrer policy is
// Always.
TEST_F(ChromeSerializedNavigationDriverTest, SanitizeWithReferrerPolicyAlways) {
  sessions::ContentSerializedNavigationDriver* driver =
      sessions::ContentSerializedNavigationDriver::GetInstance();
  sessions::SerializedNavigationEntry navigation =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  sessions::SerializedNavigationEntryTestHelper::SetReferrerPolicy(
      static_cast<int>(network::mojom::ReferrerPolicy::kAlways), &navigation);

  blink::PageState page_state =
      blink::PageState::CreateFromURL(sessions::test_data::VirtualUrl());
  sessions::SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &navigation);

  driver->Sanitize(&navigation);
  EXPECT_EQ(sessions::test_data::kIndex, navigation.index());
  EXPECT_EQ(sessions::test_data::kUniqueID, navigation.unique_id());
  EXPECT_EQ(sessions::test_data::ReferrerUrl(), navigation.referrer_url());
  EXPECT_EQ(static_cast<int>(network::mojom::ReferrerPolicy::kAlways),
            navigation.referrer_policy());
  EXPECT_EQ(sessions::test_data::VirtualUrl(), navigation.virtual_url());
  EXPECT_EQ(sessions::test_data::kTitle, navigation.title());
  EXPECT_EQ(page_state.ToEncodedData(), navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), sessions::test_data::kTransitionType));
  EXPECT_EQ(sessions::test_data::kHasPostData, navigation.has_post_data());
  EXPECT_EQ(sessions::test_data::kPostID, navigation.post_id());
  EXPECT_EQ(sessions::test_data::OriginalRequestUrl(),
            navigation.original_request_url());
  EXPECT_EQ(sessions::test_data::kIsOverridingUserAgent,
            navigation.is_overriding_user_agent());
  EXPECT_EQ(sessions::test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(sessions::test_data::FaviconUrl(), navigation.favicon_url());
  EXPECT_EQ(sessions::test_data::kHttpStatusCode,
            navigation.http_status_code());
}

// Tests that the input data is properly sanitized when the referrer policy is
// Never.
TEST_F(ChromeSerializedNavigationDriverTest, SanitizeWithReferrerPolicyNever) {
  sessions::ContentSerializedNavigationDriver* driver =
      sessions::ContentSerializedNavigationDriver::GetInstance();
  sessions::SerializedNavigationEntry navigation =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  sessions::SerializedNavigationEntryTestHelper::SetReferrerPolicy(
      static_cast<int>(network::mojom::ReferrerPolicy::kNever), &navigation);

  blink::PageState page_state =
      blink::PageState::CreateFromURL(sessions::test_data::VirtualUrl());
  sessions::SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &navigation);

  driver->Sanitize(&navigation);

  // Fields that should remain untouched.
  EXPECT_EQ(sessions::test_data::kIndex, navigation.index());
  EXPECT_EQ(sessions::test_data::kUniqueID, navigation.unique_id());
  EXPECT_EQ(sessions::test_data::VirtualUrl(), navigation.virtual_url());
  EXPECT_EQ(sessions::test_data::kTitle, navigation.title());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), sessions::test_data::kTransitionType));
  EXPECT_EQ(sessions::test_data::kHasPostData, navigation.has_post_data());
  EXPECT_EQ(sessions::test_data::kPostID, navigation.post_id());
  EXPECT_EQ(sessions::test_data::OriginalRequestUrl(),
            navigation.original_request_url());
  EXPECT_EQ(sessions::test_data::kIsOverridingUserAgent,
            navigation.is_overriding_user_agent());
  EXPECT_EQ(sessions::test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(sessions::test_data::FaviconUrl(), navigation.favicon_url());
  EXPECT_EQ(sessions::test_data::kHttpStatusCode,
            navigation.http_status_code());

  // Fields that were sanitized.
  EXPECT_EQ(GURL(), navigation.referrer_url());
  EXPECT_EQ(static_cast<int>(network::mojom::ReferrerPolicy::kDefault),
            navigation.referrer_policy());
  EXPECT_EQ(page_state.ToEncodedData(), navigation.encoded_page_state());
}

// TODO(dbeam): add tests for clearing session restore state from new Material
// Design URLs that we're pulling out of the uber page.
