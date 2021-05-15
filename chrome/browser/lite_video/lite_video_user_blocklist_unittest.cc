// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_user_blocklist.h"

#include <map>
#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace lite_video {

namespace {

// Empty mock class to test the LiteVideoUserBlocklist.
class EmptyOptOutBlocklistDelegate : public blocklist::OptOutBlocklistDelegate {
 public:
  EmptyOptOutBlocklistDelegate() = default;
};

class TestLiteVideoUserBlocklist : public LiteVideoUserBlocklist {
 public:
  TestLiteVideoUserBlocklist(
      std::unique_ptr<blocklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      blocklist::OptOutBlocklistDelegate* blocklist_delegate)
      : LiteVideoUserBlocklist(std::move(opt_out_store),
                               clock,
                               blocklist_delegate) {}
  ~TestLiteVideoUserBlocklist() override = default;

  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override {
    return LiteVideoUserBlocklist::ShouldUseSessionPolicy(duration, history,
                                                          threshold);
  }

  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override {
    return LiteVideoUserBlocklist::ShouldUsePersistentPolicy(duration, history,
                                                             threshold);
  }

  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override {
    return LiteVideoUserBlocklist::ShouldUseHostPolicy(duration, history,
                                                       threshold, max_hosts);
  }

  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override {
    return LiteVideoUserBlocklist::ShouldUseTypePolicy(duration, history,
                                                       threshold);
  }

  blocklist::BlocklistData::AllowedTypesAndVersions GetAllowedTypes()
      const override {
    return LiteVideoUserBlocklist::GetAllowedTypes();
  }
};

class LiteVideoUserBlocklistTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ConfigBlocklistWithParams({});
  }
  // Sets up a new blocklist with the given |params|.
  void ConfigBlocklistWithParams(
      const std::map<std::string, std::string>& params) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kLiteVideo, params);
    blocklist_ = std::make_unique<TestLiteVideoUserBlocklist>(
        nullptr, &test_clock_, &blocklist_delegate_);
  }

  void ConfigBlocklistParamsForTesting() {
    int max_user_blocklist_hosts = 1;
    int user_blocklist_opt_out_history_threshold = 1;

    ConfigBlocklistWithParams(
        {{"user_blocklist_opt_out_history_threshold",
          base::NumberToString(user_blocklist_opt_out_history_threshold)},
         {"max_user_blocklist_hosts",
          base::NumberToString(max_user_blocklist_hosts)}});
  }

  void SeedBlocklist(const std::string& key, LiteVideoBlocklistType type) {
    blocklist_->AddEntry(key, true, static_cast<int>(type));
  }

  LiteVideoBlocklistReason CheckBlocklistForSubframeNavigation(
      const GURL& mainframe_url,
      const GURL& subframe_url) {
    // Needed so that a mainframe navigation exists.
    NavigateAndCommit(mainframe_url);
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
    content::MockNavigationHandle navigation_handle(subframe_url, subframe);
    return blocklist_->IsLiteVideoAllowedOnNavigation(&navigation_handle);
  }

  LiteVideoBlocklistReason CheckBlocklistForMainframeNavigation(
      const GURL& url) {
    // Needed so that a mainframe navigation exists.
    NavigateAndCommit(url);
    content::MockNavigationHandle navigation_handle(url, main_rfh());
    return blocklist_->IsLiteVideoAllowedOnNavigation(&navigation_handle);
  }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

  TestLiteVideoUserBlocklist* blocklist() { return blocklist_.get(); }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

 private:
  EmptyOptOutBlocklistDelegate blocklist_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::SimpleTestClock test_clock_;
  std::unique_ptr<TestLiteVideoUserBlocklist> blocklist_;
};

TEST_F(LiteVideoUserBlocklistTest, NavigationNotEligibile) {
  GURL url("chrome://about");
  EXPECT_EQ(CheckBlocklistForMainframeNavigation(url),
            LiteVideoBlocklistReason::kNavigationNotEligibile);
}

TEST_F(LiteVideoUserBlocklistTest, NavigationBlocklistedByNavigationBlocklist) {
  ConfigBlocklistParamsForTesting();
  GURL url("https://test.com");
  content::MockNavigationHandle nav_handle;
  nav_handle.set_url(url);
  blocklist()->AddNavigationToBlocklist(&nav_handle, true);
  EXPECT_EQ(CheckBlocklistForMainframeNavigation(url),
            LiteVideoBlocklistReason::kNavigationBlocklisted);
}

TEST_F(LiteVideoUserBlocklistTest, NavigationUnblocklistedByNavigation) {
  ConfigBlocklistParamsForTesting();
  GURL url("https://test.com");
  content::MockNavigationHandle nav_handle;
  nav_handle.set_url(url);
  blocklist()->AddNavigationToBlocklist(&nav_handle, true);
  test_clock()->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(CheckBlocklistForMainframeNavigation(url),
            LiteVideoBlocklistReason::kNavigationBlocklisted);
  blocklist()->AddNavigationToBlocklist(&nav_handle, false);
  test_clock()->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(CheckBlocklistForMainframeNavigation(url),
            LiteVideoBlocklistReason::kAllowed);
}

TEST_F(LiteVideoUserBlocklistTest,
       MainframeNavigationBlocklistedByRebufferBlocklist) {
  ConfigBlocklistParamsForTesting();
  GURL url("https://test.com");
  blocklist()->AddRebufferToBlocklist(url, absl::nullopt, true);
  EXPECT_EQ(CheckBlocklistForMainframeNavigation(url),
            LiteVideoBlocklistReason::kRebufferingBlocklisted);
}

TEST_F(LiteVideoUserBlocklistTest, MainframeNavigationAllowed) {
  ConfigBlocklistParamsForTesting();
  GURL url("https://test.com");
  EXPECT_EQ(CheckBlocklistForMainframeNavigation(url),
            LiteVideoBlocklistReason::kAllowed);
}

TEST_F(LiteVideoUserBlocklistTest,
       SubframeNavigationBlocklistedByRebufferBlocklist) {
  ConfigBlocklistParamsForTesting();
  GURL mainframe_url("https://test.com");
  GURL subframe_url("https://subframe.com");
  blocklist()->AddRebufferToBlocklist(mainframe_url, subframe_url, true);
  EXPECT_EQ(CheckBlocklistForSubframeNavigation(mainframe_url, subframe_url),
            LiteVideoBlocklistReason::kRebufferingBlocklisted);
}

TEST_F(LiteVideoUserBlocklistTest, SubframeNavigationAllowed) {
  ConfigBlocklistParamsForTesting();
  GURL mainframe_url("https://test.com");
  GURL subframe_url("https://subframe.com");
  EXPECT_EQ(CheckBlocklistForSubframeNavigation(mainframe_url, subframe_url),
            LiteVideoBlocklistReason::kAllowed);
}

TEST_F(LiteVideoUserBlocklistTest, DefaultParams) {
  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;
  size_t max_hosts = 0;

  EXPECT_TRUE(blocklist()->ShouldUseHostPolicy(&duration, &history, &threshold,
                                               &max_hosts));
  EXPECT_EQ(base::TimeDelta::FromDays(1), duration);
  EXPECT_EQ(5u, history);
  EXPECT_EQ(5, threshold);
  EXPECT_EQ(50u, max_hosts);

  EXPECT_FALSE(blocklist()->ShouldUseTypePolicy(nullptr, nullptr, nullptr));

  blocklist::BlocklistData::AllowedTypesAndVersions types =
      blocklist()->GetAllowedTypes();
  EXPECT_EQ(2u, types.size());

  auto iter = types.find(
      static_cast<int>(LiteVideoBlocklistType::kNavigationBlocklist));
  EXPECT_NE(iter, types.end());
  EXPECT_EQ(0, iter->second);
  iter =
      types.find(static_cast<int>(LiteVideoBlocklistType::kRebufferBlocklist));
  EXPECT_NE(iter, types.end());
  EXPECT_EQ(0, iter->second);
}

}  // namespace

}  // namespace lite_video
