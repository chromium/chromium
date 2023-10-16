// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_test.h"

namespace {

class ChromeForTestingInfoBarTest : public InProcessBrowserTest {
 public:
  ChromeForTestingInfoBarTest() = default;
  ~ChromeForTestingInfoBarTest() override = default;

  ChromeForTestingInfoBarTest(const ChromeForTestingInfoBarTest&) = delete;
  ChromeForTestingInfoBarTest& operator=(const ChromeForTestingInfoBarTest&) =
      delete;

 protected:
  infobars::ContentInfoBarManager* GetInfoBarManagerFromTabIndex(
      int tab_index) {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ChromeForTestingInfoBarTest, InfoBarAppears) {
  infobars::ContentInfoBarManager* infobar_manager =
      GetInfoBarManagerFromTabIndex(0);

  // Verify that the info bar is shown.
  ASSERT_EQ(1u, infobar_manager->infobars().size());

  auto* test_infobar = infobar_manager->infobars()[0]->delegate();

  // Assert that it is the Chrome for Testing info bar.
  ASSERT_EQ(ConfirmInfoBarDelegate::InfoBarIdentifier::
                CHROME_FOR_TESTING_INFOBAR_DELEGATE,
            test_infobar->GetIdentifier());

  EXPECT_FALSE(test_infobar->IsCloseable());
  EXPECT_FALSE(test_infobar->ShouldAnimate());
}

IN_PROC_BROWSER_TEST_F(ChromeForTestingInfoBarTest, InfoBarAppearsInEveryTab) {
  // Open a second tab in the same window.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  const unsigned number_of_tabs = browser()->tab_strip_model()->count();
  EXPECT_EQ(2u, number_of_tabs);

  // Verify that the info bar is shown in every tab.
  for (unsigned i = 0; i < number_of_tabs; ++i) {
    infobars::ContentInfoBarManager* infobar_manager =
        GetInfoBarManagerFromTabIndex(i);
    ASSERT_EQ(1u, infobar_manager->infobars().size());

    auto* test_infobar = infobar_manager->infobars()[0]->delegate();
    ASSERT_EQ(ConfirmInfoBarDelegate::InfoBarIdentifier::
                  CHROME_FOR_TESTING_INFOBAR_DELEGATE,
              test_infobar->GetIdentifier());
  }
}
