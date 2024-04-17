// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobars_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace infobars {

namespace {

class ChromeForTestingInfoBarTest : public InProcessBrowserTest {
 public:
  ChromeForTestingInfoBarTest() = default;
  ~ChromeForTestingInfoBarTest() override = default;

  ChromeForTestingInfoBarTest(const ChromeForTestingInfoBarTest&) = delete;
  ChromeForTestingInfoBarTest& operator=(const ChromeForTestingInfoBarTest&) =
      delete;

 protected:
  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ContentInfoBarManager* GetActiveWebContentsInfoBarManager() {
    return ContentInfoBarManager::FromWebContents(GetActiveWebContents());
  }

  ContentInfoBarManager* GetInfoBarManagerFromTabIndex(int tab_index) {
    return ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  }
};

IN_PROC_BROWSER_TEST_F(ChromeForTestingInfoBarTest, InfoBarAppears) {
  ContentInfoBarManager* infobar_manager = GetInfoBarManagerFromTabIndex(0);

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
    ContentInfoBarManager* infobar_manager = GetInfoBarManagerFromTabIndex(i);
    ASSERT_EQ(1u, infobar_manager->infobars().size());

    auto* test_infobar = infobar_manager->infobars()[0]->delegate();
    ASSERT_EQ(ConfirmInfoBarDelegate::InfoBarIdentifier::
                  CHROME_FOR_TESTING_INFOBAR_DELEGATE,
              test_infobar->GetIdentifier());
  }
}

// Subclass for tests that require infobars to be disabled.
class ChromeForTestingInfoBarDisabledTest : public ChromeForTestingInfoBarTest {
 public:
  ChromeForTestingInfoBarDisabledTest() = default;

  ChromeForTestingInfoBarDisabledTest(
      const ChromeForTestingInfoBarDisabledTest&) = delete;
  ChromeForTestingInfoBarDisabledTest& operator=(
      const ChromeForTestingInfoBarDisabledTest&) = delete;

  ~ChromeForTestingInfoBarDisabledTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableInfoBars);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeForTestingInfoBarDisabledTest,
                       NoInfoBarAppearsInitially) {
  ASSERT_EQ(0u, GetInfoBarManagerFromTabIndex(0)->infobars().size());
}

IN_PROC_BROWSER_TEST_F(ChromeForTestingInfoBarDisabledTest,
                       NoInfoBarAppearsInNewTabs) {
  ASSERT_EQ(0u, GetInfoBarManagerFromTabIndex(0)->infobars().size());

  // Open a second tab in the same window.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  ASSERT_EQ(0u, GetInfoBarManagerFromTabIndex(1)->infobars().size());
}

class TestInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(ContentInfoBarManager* infobar_manager,
                     bool has_buttons,
                     bool replace_existing = false) {
    infobar_manager->AddInfoBar(
        CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
            new TestInfoBarDelegate(has_buttons))),
        replace_existing);
  }

  TestInfoBarDelegate(const TestInfoBarDelegate&) = delete;
  TestInfoBarDelegate& operator=(const TestInfoBarDelegate&) = delete;

 private:
  explicit TestInfoBarDelegate(bool has_buttons) : has_buttons_(has_buttons) {}
  ~TestInfoBarDelegate() override = default;

  InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return InfoBarIdentifier::TEST_INFOBAR;
  }

  std::u16string GetMessageText() const override { return u"mock infobar"; }

  int GetButtons() const override {
    return has_buttons_ ? BUTTON_OK | BUTTON_CANCEL : BUTTON_NONE;
  }

  std::u16string GetButtonLabel(InfoBarButton button) const override {
    return button == BUTTON_OK ? u"allow" : u"deny";
  }

  const bool has_buttons_;
};

IN_PROC_BROWSER_TEST_F(ChromeForTestingInfoBarDisabledTest,
                       ChromeForTestingInfoBarWithButtonsVisibility) {
  ContentInfoBarManager* infobar_manager = GetActiveWebContentsInfoBarManager();
  ASSERT_TRUE(infobar_manager);

  // No inforbars should be shown upon start due to --disable-infobars.
  EXPECT_THAT(infobar_manager->infobars(), testing::IsEmpty());

  // Try to add infobar with no buttons and verify it was not added.
  TestInfoBarDelegate::Create(infobar_manager, /*has_buttons=*/false);

  EXPECT_THAT(infobar_manager->infobars(), testing::IsEmpty());

  // Try to add infobar with buttons and verify it was added.
  TestInfoBarDelegate::Create(infobar_manager, /*has_buttons=*/true);

  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
  EXPECT_EQ(infobar_manager->infobars()[0]->GetIdentifier(),
            InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR);

  // Replace existing infobar with a new one that has no buttons and verify that
  // the existing infobar was removed and the new one was not added.
  TestInfoBarDelegate::Create(infobar_manager, /*has_buttons=*/false,
                              /*replace_existing=*/true);

  EXPECT_THAT(infobar_manager->infobars(), testing::IsEmpty());
}

}  // namespace

}  // namespace infobars
