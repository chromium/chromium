// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/global_confirm_info_bar.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_ui_test_utils.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

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

class GlobalConfirmInfoBarTest : public PlatformBrowserTest {
 public:
  GlobalConfirmInfoBarTest() = default;

  GlobalConfirmInfoBarTest(const GlobalConfirmInfoBarTest&) = delete;
  GlobalConfirmInfoBarTest& operator=(const GlobalConfirmInfoBarTest&) = delete;

  ~GlobalConfirmInfoBarTest() override = default;

 protected:
  infobars::ContentInfoBarManager* GetInfoBarManagerFromTabIndex(
      int tab_index) {
    return infobars::ContentInfoBarManager::FromWebContents(
        chrome_test_utils::GetWebContentsAt(this, tab_index));
  }

#if BUILDFLAG(IS_ANDROID)
  // Adds an additional tab.
  void AddTab() {
    android_ui_test_utils::OpenUrlInNewTab(
        GetProfile(), chrome_test_utils::GetActiveWebContents(this),
        GURL("chrome://blank/"));
  }

  // Returns the number of tabs in the current window.
  int GetTabCount() {
    for (const TabModel* model : TabModelList::models()) {
      if (model->IsActiveModel()) {
        return model->GetTabCount();
      }
    }
    NOTREACHED() << "No active TabModel?";
  }
#else
  // Adds an additional tab.
  void AddTab() {
    ASSERT_FALSE(
        AddTabAtIndex(0, GURL("chrome://blank/"), ui::PAGE_TRANSITION_LINK));
  }

  // Returns the number of tabs in the current window.
  int GetTabCount() { return browser()->tab_strip_model()->GetTabCount(); }
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace

IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest, UserInteraction) {
  AddTab();
  ASSERT_EQ(2, GetTabCount());

  // Make sure each tab has no info bars.
  for (int i = 0; i < GetTabCount(); i++) {
    EXPECT_EQ(0u, GetInfoBarManagerFromTabIndex(i)->infobars().size());
  }

  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>();
  TestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown on each tab.
  for (int i = 0; i < GetTabCount(); i++) {
    infobars::ContentInfoBarManager* infobar_manager =
        GetInfoBarManagerFromTabIndex(i);
    ASSERT_EQ(1u, infobar_manager->infobars().size());
    EXPECT_TRUE(infobar_manager->infobars()[0]->delegate()->EqualsDelegate(
        delegate_ptr));
  }

  // Close the GlobalConfirmInfoBar by simulating an interaction with the info
  // bar on one of the tabs. In this case, the first tab is picked.
  infobars::InfoBar* first_tab_infobar =
      GetInfoBarManagerFromTabIndex(0)->infobars()[0];
  EXPECT_TRUE(
      first_tab_infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());

  // Usually, clicking the button makes the info bar close itself if Accept()
  // returns true. In our case, since we interacted with the info bar delegate
  // directly, the info bar must be removed manually.
  first_tab_infobar->RemoveSelf();

  for (int i = 0; i < GetTabCount(); i++) {
    EXPECT_EQ(0u, GetInfoBarManagerFromTabIndex(i)->infobars().size());
  }
}

IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest, CreateAndCloseInfobar) {
  ASSERT_EQ(1, GetTabCount());
  infobars::ContentInfoBarManager* infobar_manager =
      GetInfoBarManagerFromTabIndex(0);

  // Make sure the tab has no info bar.
  EXPECT_EQ(0u, infobar_manager->infobars().size());

  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>();
  TestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  GlobalConfirmInfoBar* infobar =
      GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown.
  ASSERT_EQ(1u, infobar_manager->infobars().size());

  auto* test_infobar = infobar_manager->infobars()[0]->delegate();
  EXPECT_TRUE(test_infobar->EqualsDelegate(delegate_ptr));
  EXPECT_TRUE(test_infobar->IsCloseable());

  // Close the infobar and make sure that the tab has no info bar.
  infobar->Close();
  EXPECT_EQ(0u, infobar_manager->infobars().size());
}

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

IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest,
                       VerifyInfobarNonDefaultProperties) {
  ASSERT_EQ(1, GetTabCount());
  infobars::ContentInfoBarManager* infobar_manager =
      GetInfoBarManagerFromTabIndex(0);

  // Make sure the tab has no info bar.
  EXPECT_EQ(0u, infobar_manager->infobars().size());

  auto delegate = std::make_unique<NonDefaultTestConfirmInfoBarDelegate>();
  NonDefaultTestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown.
  ASSERT_EQ(1u, infobar_manager->infobars().size());

  auto* test_infobar = infobar_manager->infobars()[0]->delegate();
  EXPECT_TRUE(test_infobar->EqualsDelegate(delegate_ptr));

  EXPECT_FALSE(test_infobar->IsCloseable());
  EXPECT_FALSE(test_infobar->ShouldAnimate());
}

class TestConfirmInfoBarDelegateWithLink : public TestConfirmInfoBarDelegate {
 public:
  TestConfirmInfoBarDelegateWithLink() = default;

  TestConfirmInfoBarDelegateWithLink(
      const TestConfirmInfoBarDelegateWithLink&) = delete;
  TestConfirmInfoBarDelegateWithLink& operator=(
      const TestConfirmInfoBarDelegateWithLink&) = delete;

  ~TestConfirmInfoBarDelegateWithLink() override = default;

  std::u16string GetLinkText() const override { return u"Test"; }
  GURL GetLinkURL() const override { return GURL("about:blank"); }
};

// Verifies that clicking a link in a global infobar does not crash. Regression
// test for http://crbug.com/1393765.
IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest, ClickLink) {
  // Show an infobar with a link.
  ASSERT_EQ(1, GetTabCount());
  GlobalConfirmInfoBar::Show(
      std::make_unique<TestConfirmInfoBarDelegateWithLink>());

  // Simulate clicking the link on the infobar.
  infobars::InfoBar* first_tab_infobar =
      GetInfoBarManagerFromTabIndex(0)->infobars()[0];
  EXPECT_FALSE(first_tab_infobar->delegate()->LinkClicked(
      WindowOpenDisposition::NEW_BACKGROUND_TAB));

  // This should have opened a new tab.
  ASSERT_EQ(2, GetTabCount());
}
