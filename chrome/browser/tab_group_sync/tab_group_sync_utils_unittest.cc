// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using testing::Return;

namespace tab_groups {
namespace {

class TabGroupSyncUtilsTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    navigation_handle_ =
        std::make_unique<content::MockNavigationHandle>(web_contents());
    ON_CALL(*navigation_handle_, GetRequestMethod())
        .WillByDefault(Return(net::HttpRequestHeaders::kGetMethod));
    ON_CALL(*navigation_handle_, HasUserGesture()).WillByDefault(Return(true));
  }

  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
};

TEST_F(TabGroupSyncUtilsTest, FragmentChangeIsNotSaveable) {
  navigation_handle_->set_url(GURL("http://www.foo.com#1"));
  navigation_handle_->set_has_committed(true);
  navigation_handle_->set_page_transition(ui::PAGE_TRANSITION_LINK);
  navigation_handle_->set_is_same_document(true);
  navigation_handle_->set_previous_primary_main_frame_url(
      GURL("http://www.foo.com#2"));
  EXPECT_CALL(*navigation_handle_, ShouldUpdateHistory())
      .WillOnce(Return(true));
  EXPECT_FALSE(TabGroupSyncUtils::IsSaveableNavigation(
      /*is_extension_navigation_allowed=*/true, navigation_handle_.get()));
}

class TabGroupSyncUtilsResponseCodeTest
    : public TabGroupSyncUtilsTest,
      public testing::WithParamInterface<bool> {};

TEST_P(TabGroupSyncUtilsResponseCodeTest, IsSaveableNavigation404) {
  navigation_handle_->set_url(GURL("http://www.foo.com/custom404"));
  navigation_handle_->set_has_committed(true);
  navigation_handle_->set_page_transition(ui::PAGE_TRANSITION_LINK);
  std::string raw_response_headers = "HTTP/1.1 404 Not Found\r\n\r\n";
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  navigation_handle_->set_response_headers(response_headers);

  bool are_404_navigations_included_in_history = GetParam();
  EXPECT_CALL(*navigation_handle_, ShouldUpdateHistory())
      .WillOnce(Return(are_404_navigations_included_in_history));
  EXPECT_EQ(
      TabGroupSyncUtils::IsSaveableNavigation(
          /*is_extension_navigation_allowed=*/true, navigation_handle_.get()),
      are_404_navigations_included_in_history);
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabGroupSyncUtilsResponseCodeTest,
                         ::testing::Bool());

}  // namespace
}  // namespace tab_groups
