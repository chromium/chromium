// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_tab_helper.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/test/mock_mahi_web_contents_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace mahi {

using testing::_;
using testing::Return;

class MahiTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->is_mahi_enabled = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
    scoped_mahi_web_contents_manager_ =
        std::make_unique<chromeos::ScopedMahiWebContentsManagerOverride>(
            &mock_mahi_web_contents_manager_);

    // Initialize browser.
    const Browser::CreateParams params(profile(), /*user_gesture=*/true);
    browser_ = CreateBrowserWithTestWindowForParams(params);
    tab_strip_model_ = browser_->tab_strip_model();
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    tab_strip_model_ = nullptr;
    browser_.reset();

    scoped_mahi_web_contents_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  MockMahiWebContentsManager mock_mahi_web_contents_manager_;
  std::unique_ptr<chromeos::ScopedMahiWebContentsManagerOverride>
      scoped_mahi_web_contents_manager_;

  TabActivitySimulator tab_activity_simulator_;
  raw_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(MahiTabHelperTest, FocusedTabLoadComplete) {
  // Don't get notifications from unfocused tab.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(0);
  MahiTabHelper::CreateForWebContents(web_contents());
  EXPECT_NE(nullptr, MahiTabHelper::FromWebContents(web_contents()));
  NavigateAndCommit(GURL("https://example1.com"));

  // When a tab gets focus, notification will be received from navigation.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(2);
  MahiTabHelper::FromWebContents(web_contents())->OnWebContentsFocused(nullptr);
  NavigateAndCommit(GURL("https://example2.com"));

  // After losing focus, the tab's notification will no longer be received.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(0);
  MahiTabHelper::FromWebContents(web_contents())
      ->OnWebContentsLostFocus(nullptr);
  NavigateAndCommit(GURL("https://example3.com"));
}

TEST_F(MahiTabHelperTest, TabSwitch) {
  MahiTabHelper::CreateForWebContents(web_contents());
  NavigateAndCommit(GURL("https://example1.com"));

  content::WebContents* web_contents2 =
      tab_activity_simulator_.AddWebContentsAndNavigate(
          tab_strip_model_, GURL("https://example2.com"));

  EXPECT_NE(nullptr, MahiTabHelper::FromWebContents(web_contents()));
  EXPECT_NE(nullptr, MahiTabHelper::FromWebContents(web_contents2));

  // Switch back to a previous loaded tab.
  EXPECT_CALL(mock_mahi_web_contents_manager_, OnFocusedPageLoadComplete(_))
      .Times(1);
  // Change active tab with `browser()->tab_strip_model()->ActivateTabAt()` or
  // `AddPage()` will not trigger focus events. Fire it manually instead.
  MahiTabHelper::FromWebContents(web_contents())->OnWebContentsFocused(nullptr);
}

}  // namespace mahi
