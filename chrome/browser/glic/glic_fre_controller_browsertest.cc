// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/auth_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicFreControllerBrowserTest : public InProcessBrowserTest {
 public:
  GlicFreControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  GlicFreControllerBrowserTest(const GlicFreControllerBrowserTest&) = delete;
  GlicFreControllerBrowserTest& operator=(const GlicFreControllerBrowserTest&) =
      delete;

  ~GlicFreControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_env_ = std::make_unique<signin::IdentityTestEnvironment>();

    glic_fre_controller_ = std::make_unique<GlicFreController>(
        browser()->profile(), identity_env_->identity_manager());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    glic_fre_controller_.reset();
  }

  GlicFreController* glic_fre_controller() {
    return glic_fre_controller_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_env_;
  std::unique_ptr<GlicFreController> glic_fre_controller_;
};

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       GlicFreDialogBlockedByModalUI) {
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(tab_web_contents);

  // FRE dialog should be blocked from showing if another modal dialog is
  // already open.
  auto scoped_tab_modal_ui = tab_interface->ShowModalUI();
  EXPECT_FALSE(glic_fre_controller()->CanShowFreDialog(browser()));

  // The FRE dialog should be able to open after the existing modal dialog
  // is closed.
  scoped_tab_modal_ui.reset();
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());

  // Verify the FRE dialog is shown.
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       GlicFreDialogFollowsModalUI) {
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(tab_web_contents);

  // The FRE dialog should be able to open with no other modal dialogs open.
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());

  // Verify the FRE dialog is shown.
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());

  // Verify that another modal dialog cannot be shown now that the FRE is open.
  EXPECT_FALSE(tab_interface->CanShowModalUI());

  // Once the FRE is closed, other modal dialogs can be shown again.
  glic_fre_controller()->DismissFre();
  EXPECT_TRUE(tab_interface->CanShowModalUI());
}

}  // namespace
}  // namespace glic
