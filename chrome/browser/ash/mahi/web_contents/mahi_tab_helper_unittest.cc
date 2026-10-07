// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/web_contents/mahi_tab_helper.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ash/mahi/web_contents/test_support/mock_mahi_web_contents_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mahi {

using testing::_;
using testing::Return;

class MahiTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
    scoped_mahi_web_contents_manager_ =
        std::make_unique<chromeos::ScopedMahiWebContentsManagerOverride>(
            &mock_mahi_web_contents_manager_);
  }

  void TearDown() override {
    scoped_mahi_web_contents_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  MockMahiWebContentsManager mock_mahi_web_contents_manager_;
  std::unique_ptr<chromeos::ScopedMahiWebContentsManagerOverride>
      scoped_mahi_web_contents_manager_;
};

TEST_F(MahiTabHelperTest, FocusedTabLoadComplete) {
  // Don't get notifications from unfocused tab.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(0);
  std::unique_ptr<MahiTabHelper> tab_helper =
      MahiTabHelper::MaybeCreate(web_contents());
  ASSERT_NE(nullptr, tab_helper);
  NavigateAndCommit(GURL("https://example1.com"));

  // When a tab gets focus, notification will be received from navigation.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(2);
  tab_helper->OnWebContentsFocused(nullptr);
  NavigateAndCommit(GURL("https://example2.com"));

  // After losing focus, the tab's notification will no longer be received.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(0);
  tab_helper->OnWebContentsLostFocus(nullptr);
  NavigateAndCommit(GURL("https://example3.com"));
}

TEST_F(MahiTabHelperTest, TabSwitch) {
  std::unique_ptr<MahiTabHelper> tab_helper =
      MahiTabHelper::MaybeCreate(web_contents());
  ASSERT_NE(nullptr, tab_helper);
  NavigateAndCommit(GURL("https://example1.com"));

  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();
  std::unique_ptr<MahiTabHelper> tab_helper2 =
      MahiTabHelper::MaybeCreate(web_contents2.get());
  ASSERT_NE(nullptr, tab_helper2);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents2.get(), GURL("https://example2.com"));

  // Switch back to a previous loaded tab.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(1);
  // Change active tab with `browser()->tab_strip_model()->ActivateTabAt()` or
  // `AddPage()` will not trigger focus events. Fire it manually instead.
  tab_helper->OnWebContentsFocused(nullptr);
}

}  // namespace mahi
