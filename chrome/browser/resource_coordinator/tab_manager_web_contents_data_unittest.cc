// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebContentsTester;

namespace resource_coordinator {
namespace {

constexpr TabLoadTracker::LoadingState UNLOADED =
    TabLoadTracker::LoadingState::UNLOADED;
constexpr TabLoadTracker::LoadingState LOADING =
    TabLoadTracker::LoadingState::LOADING;
constexpr TabLoadTracker::LoadingState LOADED =
    TabLoadTracker::LoadingState::LOADED;

class TabManagerWebContentsDataTest : public ChromeRenderViewHostTestHarness {
 public:
  TabManagerWebContentsDataTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Fast-forward time to prevent the first call to NowTicks() in a test from
    // returning a null TimeTicks.
    test_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_data_ = CreateWebContentsAndTabData(&web_contents_);
  }

  void TearDown() override {
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TabManager::WebContentsData* tab_data() { return tab_data_; }

  base::SimpleTestTickClock& test_clock() { return test_clock_; }

  TabManager::WebContentsData* CreateWebContentsAndTabData(
      std::unique_ptr<WebContents>* web_contents) {
    *web_contents =
        WebContentsTester::CreateTestWebContents(browser_context(), nullptr);
    TabManager::WebContentsData::CreateForWebContents(web_contents->get());
    return TabManager::WebContentsData::FromWebContents(web_contents->get());
  }

 private:
  std::unique_ptr<WebContents> web_contents_;
  TabManager::WebContentsData* tab_data_;
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
};

const char kDefaultUrl[] = "https://www.google.com";

}  // namespace

TEST_F(TabManagerWebContentsDataTest, TabLoadingState) {
  EXPECT_EQ(UNLOADED, tab_data()->tab_loading_state());
  tab_data()->SetTabLoadingState(LOADING);
  EXPECT_EQ(LOADING, tab_data()->tab_loading_state());
  tab_data()->SetTabLoadingState(LOADED);
  EXPECT_EQ(LOADED, tab_data()->tab_loading_state());
}

TEST_F(TabManagerWebContentsDataTest, CopyState) {
  tab_data()->SetTabLoadingState(LOADED);
  tab_data()->SetIsInSessionRestore(true);
  tab_data()->SetIsRestoredInForeground(true);

  std::unique_ptr<WebContents> web_contents2;
  auto* tab_data2 = CreateWebContentsAndTabData(&web_contents2);

  EXPECT_NE(tab_data()->tab_data_, tab_data2->tab_data_);
  TabManager::WebContentsData::CopyState(tab_data()->web_contents(),
                                         tab_data2->web_contents());
  EXPECT_EQ(tab_data()->tab_data_, tab_data2->tab_data_);
}

TEST_F(TabManagerWebContentsDataTest, IsInSessionRestoreWithTabLoading) {
  EXPECT_FALSE(tab_data()->is_in_session_restore());
  tab_data()->SetIsInSessionRestore(true);
  EXPECT_TRUE(tab_data()->is_in_session_restore());

  WebContents* contents = tab_data()->web_contents();
  WebContentsTester::For(contents)->NavigateAndCommit(GURL(kDefaultUrl));
  WebContentsTester::For(contents)->TestSetIsLoading(false);
  EXPECT_FALSE(tab_data()->is_in_session_restore());
}

TEST_F(TabManagerWebContentsDataTest, IsInSessionRestoreWithTabClose) {
  EXPECT_FALSE(tab_data()->is_in_session_restore());
  tab_data()->SetIsInSessionRestore(true);
  EXPECT_TRUE(tab_data()->is_in_session_restore());

  tab_data()->WebContentsDestroyed();
  EXPECT_FALSE(tab_data()->is_in_session_restore());
}

TEST_F(TabManagerWebContentsDataTest, IsTabRestoredInForeground) {
  EXPECT_FALSE(tab_data()->is_restored_in_foreground());
  tab_data()->SetIsRestoredInForeground(true);
  EXPECT_TRUE(tab_data()->is_restored_in_foreground());
}

}  // namespace resource_coordinator
