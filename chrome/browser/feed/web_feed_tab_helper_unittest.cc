// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/web_feed_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

const char kWebFeedId[] = "webfeedid";

class TestWebFeedInfoFinder : public WebFeedTabHelper::WebFeedInfoFinder {
 public:
  TestWebFeedInfoFinder() = default;
  ~TestWebFeedInfoFinder() override = default;

  void FindForPage(
      content::WebContents* web_contents,
      base::OnceCallback<void(WebFeedMetadata)> callback) override {
    WebFeedMetadata metadata;
    metadata.web_feed_id = web_feed_id_;
    metadata.subscription_status = subscription_status_;
    std::move(callback).Run(metadata);
  }

  void set_web_feed_id(const std::string& web_feed_id) {
    web_feed_id_ = web_feed_id;
  }

  void set_subscription_status(WebFeedSubscriptionStatus subscription_status) {
    subscription_status_ = subscription_status;
  }

 private:
  std::string web_feed_id_;
  WebFeedSubscriptionStatus subscription_status_ =
      WebFeedSubscriptionStatus::kUnknown;
};

}  // namespace

class WebFeedTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  WebFeedTabHelperTest();

  WebFeedTabHelperTest(const WebFeedTabHelperTest&) = delete;
  WebFeedTabHelperTest& operator=(const WebFeedTabHelperTest&) = delete;

  ~WebFeedTabHelperTest() override = default;

  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override;

  void CreateNavigationSimulator(const GURL& url);

  WebFeedTabHelper* tab_helper() const { return tab_helper_; }
  TestWebFeedInfoFinder* web_feed_info_finder() const {
    return web_feed_info_finder_;
  }
  content::NavigationSimulator* navigation_simulator() const {
    return navigation_simulator_.get();
  }

 protected:
  const GURL kPageURL = GURL("http://sample.org");
  const GURL kPageURL2 = GURL("http://test.org");

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<WebFeedTabHelper> tab_helper_;  // Owned by WebContents.
  raw_ptr<TestWebFeedInfoFinder>
      web_feed_info_finder_;  // Owned by WebFeedTabHelper.
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  base::WeakPtrFactory<WebFeedTabHelperTest> weak_ptr_factory_{this};
};

WebFeedTabHelperTest::WebFeedTabHelperTest() : tab_helper_(nullptr) {}

void WebFeedTabHelperTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  feature_list_.InitAndEnableFeature(feed::kWebUiFeed);
  WebFeedTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = WebFeedTabHelper::FromWebContents(web_contents());
  std::unique_ptr<TestWebFeedInfoFinder> web_feed_info_finder =
      std::make_unique<TestWebFeedInfoFinder>();
  web_feed_info_finder_ = web_feed_info_finder.get();
  tab_helper_->SetWebFeedInfoFinderForTesting(std::move(web_feed_info_finder));
}

void WebFeedTabHelperTest::TearDown() {
  content::RenderViewHostTestHarness::TearDown();
}

std::unique_ptr<content::BrowserContext>
WebFeedTabHelperTest::CreateBrowserContext() {
  return TestingProfile::Builder().Build();
}

void WebFeedTabHelperTest::CreateNavigationSimulator(const GURL& url) {
  navigation_simulator_ =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation_simulator_->SetTransition(ui::PAGE_TRANSITION_LINK);
}

TEST_F(WebFeedTabHelperTest, Followed) {
  web_feed_info_finder()->set_web_feed_id(kWebFeedId);
  web_feed_info_finder()->set_subscription_status(
      WebFeedSubscriptionStatus::kSubscribed);

  CreateNavigationSimulator(kPageURL);
  EXPECT_NE(nullptr, tab_helper());

  navigation_simulator()->Start();
  navigation_simulator()->Commit();
  EXPECT_EQ(kPageURL, tab_helper()->url());
  EXPECT_EQ(TabWebFeedFollowState::kFollowed, tab_helper()->follow_state());
  EXPECT_EQ(kWebFeedId, tab_helper()->web_feed_id());
}

TEST_F(WebFeedTabHelperTest, NotFollowed) {
  web_feed_info_finder()->set_subscription_status(
      WebFeedSubscriptionStatus::kNotSubscribed);

  CreateNavigationSimulator(kPageURL);
  EXPECT_NE(nullptr, tab_helper());

  navigation_simulator()->Start();
  navigation_simulator()->Commit();
  EXPECT_EQ(kPageURL, tab_helper()->url());
  EXPECT_EQ(TabWebFeedFollowState::kNotFollowed, tab_helper()->follow_state());
  EXPECT_TRUE(tab_helper()->web_feed_id().empty());
}

TEST_F(WebFeedTabHelperTest, Unknown) {
  web_feed_info_finder()->set_subscription_status(
      WebFeedSubscriptionStatus::kSubscribeInProgress);

  CreateNavigationSimulator(kPageURL);
  EXPECT_NE(nullptr, tab_helper());

  navigation_simulator()->Start();
  navigation_simulator()->Commit();
  EXPECT_EQ(kPageURL, tab_helper()->url());
  EXPECT_EQ(TabWebFeedFollowState::kUnknown, tab_helper()->follow_state());
  EXPECT_TRUE(tab_helper()->web_feed_id().empty());
}

TEST_F(WebFeedTabHelperTest, UpdateWebFeedInfo) {
  web_feed_info_finder()->set_subscription_status(
      WebFeedSubscriptionStatus::kNotSubscribed);

  CreateNavigationSimulator(kPageURL);
  EXPECT_NE(nullptr, tab_helper());

  navigation_simulator()->Start();
  navigation_simulator()->Commit();
  EXPECT_EQ(kPageURL, tab_helper()->url());
  EXPECT_EQ(TabWebFeedFollowState::kNotFollowed, tab_helper()->follow_state());
  EXPECT_TRUE(tab_helper()->web_feed_id().empty());

  // The update will be performed if the url matches.
  tab_helper()->UpdateWebFeedInfo(kPageURL, TabWebFeedFollowState::kFollowed,
                                  kWebFeedId);
  EXPECT_EQ(kPageURL, tab_helper()->url());
  EXPECT_EQ(TabWebFeedFollowState::kFollowed, tab_helper()->follow_state());
  EXPECT_EQ(kWebFeedId, tab_helper()->web_feed_id());

  // The update will be skipped if the url doesn't match.
  tab_helper()->UpdateWebFeedInfo(
      kPageURL2, TabWebFeedFollowState::kNotFollowed, std::string());
  EXPECT_EQ(kPageURL, tab_helper()->url());
  EXPECT_EQ(TabWebFeedFollowState::kFollowed, tab_helper()->follow_state());
  EXPECT_EQ(kWebFeedId, tab_helper()->web_feed_id());
}

}  // namespace feed
