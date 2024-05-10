// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace chromeos {

class PrintViewManagerCrosTest : public BrowserWithTestWindowTest {
 public:
  PrintViewManagerCrosTest() = default;
  PrintViewManagerCrosTest(const PrintViewManagerCrosTest&) = delete;
  PrintViewManagerCrosTest& operator=(const PrintViewManagerCrosTest&) = delete;
  ~PrintViewManagerCrosTest() override {}

  // Overridden from testing::Test
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            ::features::kPrintPreviewCrosPrimary,
        },
        /*disabled_features=*/{});
    BrowserWithTestWindowTest::SetUp();
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrintViewManagerCrosTest, UseCrosViewManager) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                   ui::DomCode::US_P, ui::VKEY_P, /*control=*/true,
                   /*shift=*/false, /*alt=*/false, /*command=*/false);
  ASSERT_TRUE(PrintViewManagerCros::FromWebContents(web_contents));
  ASSERT_FALSE(printing::PrintViewManager::FromWebContents(web_contents));
}

TEST_F(PrintViewManagerCrosTest, UseBrowserViewManager) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{::features::kPrintPreviewCrosPrimary});
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                   ui::DomCode::US_P, ui::VKEY_P, /*control=*/true,
                   /*shift=*/false, /*alt=*/false, /*command=*/false);
  ASSERT_FALSE(PrintViewManagerCros::FromWebContents(web_contents));
  ASSERT_TRUE(printing::PrintViewManager::FromWebContents(web_contents));
}

TEST_F(PrintViewManagerCrosTest, PrintPreviewNow) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                   ui::DomCode::US_P, ui::VKEY_P, /*control=*/true,
                   /*shift=*/false, /*alt=*/false, /*command=*/false);
  auto* view_manager = PrintViewManagerCros::FromWebContents(web_contents);
  ASSERT_TRUE(view_manager);

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  view_manager->PrintPreviewNow(rfh, /*has_selection=*/true);
  EXPECT_EQ(rfh, view_manager->render_frame_host_for_testing());
  view_manager->PrintPreviewDone();
  // After cleaning up the rfh pointer, assert it is null and not a dangling
  // pointer.
  ASSERT_FALSE(view_manager->render_frame_host_for_testing());
}

}  //  namespace chromeos
