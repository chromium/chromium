// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/global_confirm_info_bar.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

class TestConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  TestConfirmInfoBarDelegate() = default;

  TestConfirmInfoBarDelegate(const TestConfirmInfoBarDelegate&) = delete;
  TestConfirmInfoBarDelegate& operator=(const TestConfirmInfoBarDelegate&) =
      delete;

  ~TestConfirmInfoBarDelegate() override = default;

  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }

  std::u16string GetMessageText() const override {
    return u"GlobalConfirmInfoBar browser tests delegate.";
  }
};

class NonDefaultTestConfirmInfoBarDelegate : public TestConfirmInfoBarDelegate {
 public:
  NonDefaultTestConfirmInfoBarDelegate() = default;

  NonDefaultTestConfirmInfoBarDelegate(
      const NonDefaultTestConfirmInfoBarDelegate&) = delete;
  NonDefaultTestConfirmInfoBarDelegate& operator=(
      const NonDefaultTestConfirmInfoBarDelegate&) = delete;

  ~NonDefaultTestConfirmInfoBarDelegate() override = default;

  bool IsCloseable() const override { return false; }
  bool ShouldAnimate() const override { return false; }
};

class DefaultTestConfirmInfoBarDelegate : public TestConfirmInfoBarDelegate {
 public:
  DefaultTestConfirmInfoBarDelegate() = default;

  DefaultTestConfirmInfoBarDelegate(const DefaultTestConfirmInfoBarDelegate&) =
      delete;
  DefaultTestConfirmInfoBarDelegate& operator=(
      const DefaultTestConfirmInfoBarDelegate&) = delete;

  ~DefaultTestConfirmInfoBarDelegate() override = default;
};

class GlobalConfirmInfoBarTest : public InProcessBrowserTest {
 public:
  GlobalConfirmInfoBarTest() = default;

  GlobalConfirmInfoBarTest(const GlobalConfirmInfoBarTest&) = delete;
  GlobalConfirmInfoBarTest& operator=(const GlobalConfirmInfoBarTest&) = delete;

  ~GlobalConfirmInfoBarTest() override = default;

 protected:
  infobars::ContentInfoBarManager* GetInfoBarManagerFromTabIndex(
      int tab_index) {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  }

  // Adds an additional tab.
  void AddTab() {
    ASSERT_FALSE(
        AddTabAtIndex(0, GURL("chrome://blank/"), ui::PAGE_TRANSITION_LINK));
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest, UserInteraction) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  AddTab();
  ASSERT_EQ(2, tab_strip_model->count());

  // Make sure each tab has no info bars.
  for (int i = 0; i < tab_strip_model->count(); i++)
    EXPECT_EQ(0u, GetInfoBarManagerFromTabIndex(i)->infobar_count());

  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>();
  TestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown on each tab.
  for (int i = 0; i < tab_strip_model->count(); i++) {
    infobars::ContentInfoBarManager* infobar_manager =
        GetInfoBarManagerFromTabIndex(i);
    ASSERT_EQ(1u, infobar_manager->infobar_count());
    EXPECT_TRUE(infobar_manager->infobar_at(0)->delegate()->EqualsDelegate(
        delegate_ptr));
  }

  // Close the GlobalConfirmInfoBar by simulating an interaction with the info
  // bar on one of the tabs. In this case, the first tab is picked.
  infobars::InfoBar* first_tab_infobar =
      GetInfoBarManagerFromTabIndex(0)->infobar_at(0);
  EXPECT_TRUE(
      first_tab_infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());

  // Usually, clicking the button makes the info bar close itself if Accept()
  // returns true. In our case, since we interacted with the info bar delegate
  // directly, the info bar must be removed manually.
  first_tab_infobar->RemoveSelf();

  for (int i = 0; i < tab_strip_model->count(); i++)
    EXPECT_EQ(0u, GetInfoBarManagerFromTabIndex(i)->infobar_count());
}

IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest, CreateAndCloseInfobar) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  infobars::ContentInfoBarManager* infobar_manager =
      GetInfoBarManagerFromTabIndex(0);

  // Make sure the tab has no info bar.
  EXPECT_EQ(0u, infobar_manager->infobar_count());

  auto delegate = std::make_unique<DefaultTestConfirmInfoBarDelegate>();
  DefaultTestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  GlobalConfirmInfoBar* infobar =
      GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown.
  ASSERT_EQ(1u, infobar_manager->infobar_count());

  auto* test_infobar = infobar_manager->infobar_at(0)->delegate();
  EXPECT_TRUE(test_infobar->EqualsDelegate(delegate_ptr));
  EXPECT_TRUE(test_infobar->IsCloseable());

  // Close the infobar and make sure that the tab has no info bar.
  infobar->Close();
  EXPECT_EQ(0u, infobar_manager->infobar_count());
}

IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest,
                       VerifyInfobarNonDefaultProperties) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  infobars::ContentInfoBarManager* infobar_manager =
      GetInfoBarManagerFromTabIndex(0);

  // Make sure the tab has no info bar.
  EXPECT_EQ(0u, infobar_manager->infobar_count());

  auto delegate = std::make_unique<NonDefaultTestConfirmInfoBarDelegate>();
  NonDefaultTestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown.
  ASSERT_EQ(1u, infobar_manager->infobar_count());

  auto* test_infobar = infobar_manager->infobar_at(0)->delegate();
  EXPECT_TRUE(test_infobar->EqualsDelegate(delegate_ptr));

  EXPECT_FALSE(test_infobar->IsCloseable());
  EXPECT_FALSE(test_infobar->ShouldAnimate());
}
