// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace infobars {

class BrowserInfoBarManagerBrowserTest : public InProcessBrowserTest {
 public:
  BrowserInfoBarManagerBrowserTest() {
    feature_list_.InitAndEnableFeature(kCentralizedInfoBarFramework);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (!BrowserInfoBarManager::From(g_browser_process)) {
      owned_manager_ =
          std::make_unique<BrowserInfoBarManager>(g_browser_process);
    }
  }

  void TearDownOnMainThread() override {
    owned_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BrowserInfoBarManager* manager() {
    return BrowserInfoBarManager::From(g_browser_process);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<BrowserInfoBarManager> owned_manager_;
};

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       RegisterAndShowCurrentTab) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kTab)
                  .Build();

  manager()->Register(std::move(spec));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_EQ(0u, infobar_manager->infobars().size());

  manager()->Show(web_contents, identifier);

  // Now one infobar should be present.
  EXPECT_EQ(1u, infobar_manager->infobars().size());
  if (infobar_manager->infobars().size() > 0) {
    EXPECT_EQ(identifier,
              infobar_manager->infobars()[0]->delegate()->GetIdentifier());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       RegisterShowAndHideCurrentTab) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kTab)
                  .Build();

  manager()->Register(std::move(spec));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);

  manager()->Show(web_contents, identifier);
  EXPECT_EQ(1u, infobar_manager->infobars().size());

  manager()->Hide(identifier);
  EXPECT_EQ(0u, infobar_manager->infobars().size());
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest, ButtonConfiguration) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  bool ok_called = false;
  auto spec =
      InfoBarSpec::Builder(identifier)
          .SetMessageText(u"Test Message")
          .SetScope(InfoBarScope::kTab)
          .AddOkButton(u"Custom OK",
                       base::BindLambdaForTesting(
                           [&](content::WebContents*) { ok_called = true; }))
          // Intentionally omitting the cancel button.
          .Build();

  manager()->Register(std::move(spec));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);

  manager()->Show(web_contents, identifier);
  ASSERT_EQ(1u, infobar_manager->infobars().size());

  auto* delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);

  // Since we only added an OK button, the Cancel button should not be present.
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK, delegate->GetButtons());
  EXPECT_EQ(u"Custom OK",
            delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));

  // Simulating an accept action.
  EXPECT_TRUE(delegate->Accept());
  EXPECT_TRUE(ok_called);
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       RegisterAndShowGlobal) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kGlobal)
                  .Build();

  manager()->Register(std::move(spec));

  // 1. Show on current browser.
  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager1 =
      ContentInfoBarManager::FromWebContents(web_contents1);
  ASSERT_EQ(0u, infobar_manager1->infobars().size());

  manager()->ShowGlobally(identifier);
  EXPECT_EQ(1u, infobar_manager1->infobars().size());

  // 2. Open a new browser window.
  Browser* browser2 = CreateBrowser(browser()->GetProfile());
  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager2 =
      ContentInfoBarManager::FromWebContents(web_contents2);

  // It should automatically have the infobar.
  EXPECT_EQ(1u, infobar_manager2->infobars().size());
  if (infobar_manager2->infobars().size() > 0) {
    EXPECT_EQ(identifier,
              infobar_manager2->infobars()[0]->delegate()->GetIdentifier());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       GlobalFollowsActiveTab) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kGlobal)
                  .Build();

  manager()->Register(std::move(spec));

  // Show the infobar.
  manager()->ShowGlobally(identifier);

  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager1 =
      ContentInfoBarManager::FromWebContents(web_contents1);
  EXPECT_EQ(1u, infobar_manager1->infobars().size());

  // Open a new tab (this will make it active).
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  content::WebContents* web_contents2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents1, web_contents2);

  auto* infobar_manager2 =
      ContentInfoBarManager::FromWebContents(web_contents2);

  // The new active tab should have the infobar.
  EXPECT_EQ(1u, infobar_manager2->infobars().size());

  // The old active tab should NOT have the infobar anymore.
  EXPECT_EQ(0u, infobar_manager1->infobars().size());
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       GlobalDismissalCascade) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kGlobal)
                  .Build();

  manager()->Register(std::move(spec));

  manager()->ShowGlobally(identifier);

  // Open a second browser.
  Browser* browser2 = CreateBrowser(browser()->GetProfile());

  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager1 =
      ContentInfoBarManager::FromWebContents(web_contents1);

  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager2 =
      ContentInfoBarManager::FromWebContents(web_contents2);

  ASSERT_EQ(1u, infobar_manager1->infobars().size());
  ASSERT_EQ(1u, infobar_manager2->infobars().size());

  // Manually dismiss the infobar on browser2.
  infobar_manager2->RemoveInfoBar(infobar_manager2->infobars()[0]);

  // It should be removed from browser1 as well (global cascade).
  EXPECT_EQ(0u, infobar_manager1->infobars().size());
  EXPECT_EQ(0u, infobar_manager2->infobars().size());
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest, FullscreenHiding) {
  // 1. Register a spec with should_hide_in_fullscreen = true.
  auto spec_hide = InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR)
                       .SetMessageText(u"Hide in fullscreen")
                       .SetShouldHideInFullscreen(true)
                       .Build();
  manager()->Register(std::move(spec_hide));

  // 2. Register a spec with should_hide_in_fullscreen = false.
  // We use a different identifier.
  auto spec_show =
      InfoBarSpec::Builder(InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE)
          .SetMessageText(u"Show in fullscreen")
          // should_hide_in_fullscreen defaults to false
          .Build();
  manager()->Register(std::move(spec_show));

  // 3. Show them.
  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  manager()->Show(web_contents1, InfoBarDelegate::TEST_INFOBAR);
  manager()->Show(web_contents1, InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_EQ(2u, infobar_manager->infobars().size());

  infobars::InfoBar* ib_hide = nullptr;
  infobars::InfoBar* ib_show = nullptr;
  for (infobars::InfoBar* ib : infobar_manager->infobars()) {
    if (ib->delegate()->GetIdentifier() == InfoBarDelegate::TEST_INFOBAR) {
      ib_hide = ib;
    } else if (ib->delegate()->GetIdentifier() ==
               InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE) {
      ib_show = ib;
    }
  }
  ASSERT_TRUE(ib_hide);
  ASSERT_TRUE(ib_show);

  // 4. Verify their ShouldHideInFullscreen() implementation.
  EXPECT_TRUE(ib_hide->delegate()->ShouldHideInFullscreen());
  EXPECT_FALSE(ib_show->delegate()->ShouldHideInFullscreen());
}

}  // namespace infobars
