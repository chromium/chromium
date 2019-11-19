// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/global_confirm_info_bar.h"

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/test_utils.h"

namespace {

class TestConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  TestConfirmInfoBarDelegate() = default;
  ~TestConfirmInfoBarDelegate() override = default;

  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }

  base::string16 GetMessageText() const override {
    return base::ASCIIToUTF16("GlobalConfirmInfoBar browser tests delegate.");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConfirmInfoBarDelegate);
};

class GlobalConfirmInfoBarTest : public InProcessBrowserTest {
 public:
  GlobalConfirmInfoBarTest() = default;
  ~GlobalConfirmInfoBarTest() override = default;

 protected:
  InfoBarService* GetInfoBarServiceFromTabIndex(int tab_index) {
    return InfoBarService::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  }

  // Adds an additional tab.
  void AddTab() {
    AddTabAtIndex(0, GURL("chrome://blank/"), ui::PAGE_TRANSITION_LINK);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobalConfirmInfoBarTest);
};

}  // namespace

// Creates a global confirm info bar on a browser with 2 tabs and closes it.
IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest, MultipleTabs) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  AddTab();
  ASSERT_EQ(2, tab_strip_model->count());

  // Make sure each tab has no info bars.
  for (int i = 0; i < tab_strip_model->count(); i++)
    EXPECT_EQ(0u, GetInfoBarServiceFromTabIndex(i)->infobar_count());

  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>();
  TestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  base::WeakPtr<GlobalConfirmInfoBar> global_confirm_info_bar =
      GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown on each tab.
  for (int i = 0; i < tab_strip_model->count(); i++) {
    InfoBarService* infobar_service = GetInfoBarServiceFromTabIndex(i);
    ASSERT_EQ(1u, infobar_service->infobar_count());
    EXPECT_TRUE(infobar_service->infobar_at(0)->delegate()->EqualsDelegate(
        delegate_ptr));
  }

  EXPECT_TRUE(global_confirm_info_bar);
  global_confirm_info_bar->Close();

  EXPECT_FALSE(global_confirm_info_bar);
  for (int i = 0; i < tab_strip_model->count(); i++)
    EXPECT_EQ(0u, GetInfoBarServiceFromTabIndex(i)->infobar_count());
}

IN_PROC_BROWSER_TEST_F(GlobalConfirmInfoBarTest, UserInteraction) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  AddTab();
  ASSERT_EQ(2, tab_strip_model->count());

  // Make sure each tab has no info bars.
  for (int i = 0; i < tab_strip_model->count(); i++)
    EXPECT_EQ(0u, GetInfoBarServiceFromTabIndex(i)->infobar_count());

  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>();
  TestConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  base::WeakPtr<GlobalConfirmInfoBar> global_confirm_info_bar =
      GlobalConfirmInfoBar::Show(std::move(delegate));

  // Verify that the info bar is shown on each tab.
  for (int i = 0; i < tab_strip_model->count(); i++) {
    InfoBarService* infobar_service = GetInfoBarServiceFromTabIndex(i);
    ASSERT_EQ(1u, infobar_service->infobar_count());
    EXPECT_TRUE(infobar_service->infobar_at(0)->delegate()->EqualsDelegate(
        delegate_ptr));
  }

  // Close the GlobalConfirmInfoBar by simulating an interaction with the info
  // bar on one of the tabs. In this case, the first tab is picked.
  infobars::InfoBar* first_tab_infobar =
      GetInfoBarServiceFromTabIndex(0)->infobar_at(0);
  EXPECT_TRUE(
      first_tab_infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());

  // Usually, clicking the button makes the info bar close itself if Accept()
  // returns true. In our case, since we interacted with the info bar delegate
  // directly, the info bar must be removed manually
  first_tab_infobar->RemoveSelf();

  EXPECT_FALSE(global_confirm_info_bar);
  for (int i = 0; i < tab_strip_model->count(); i++)
    EXPECT_EQ(0u, GetInfoBarServiceFromTabIndex(i)->infobar_count());
}
