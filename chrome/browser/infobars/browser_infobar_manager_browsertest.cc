// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include <optional>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace infobars {

namespace {

InfoBarSpec BuildGlobalSpecSkippingIncognito() {
  return InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR)
      .SetMessageText(u"Test Message")
      .SetScope(InfoBarScope::kGlobal)
      .SetBrowserFilter(
          base::BindRepeating([](BrowserWindowInterface* browser) {
            return !browser->GetProfile()->IsOffTheRecord();
          }))
      .Build();
}

size_t InfoBarCountIn(Browser* browser) {
  return ContentInfoBarManager::FromWebContents(
             browser->tab_strip_model()->GetActiveWebContents())
      ->infobars()
      .size();
}

}  // namespace

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

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  auto* infobar_manager =
      ContentInfoBarManager::FromWebContents(tab->GetContents());
  ASSERT_EQ(0u, infobar_manager->infobars().size());

  manager()->Show(tab, identifier);

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

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  auto* infobar_manager =
      ContentInfoBarManager::FromWebContents(tab->GetContents());

  manager()->Show(tab, identifier);
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

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  auto* infobar_manager =
      ContentInfoBarManager::FromWebContents(tab->GetContents());

  manager()->Show(tab, identifier);
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

  auto has_test_infobar = [](ContentInfoBarManager* manager) {
    for (const auto& ib : manager->infobars()) {
      if (ib->delegate()->GetIdentifier() == InfoBarDelegate::TEST_INFOBAR) {
        return true;
      }
    }
    return false;
  };

  ASSERT_TRUE(has_test_infobar(infobar_manager1));
  ASSERT_TRUE(has_test_infobar(infobar_manager2));

  // Find the test infobar on browser2 to remove it.
  infobars::InfoBar* test_ib2 = nullptr;
  for (const auto& ib : infobar_manager2->infobars()) {
    if (ib->delegate()->GetIdentifier() == InfoBarDelegate::TEST_INFOBAR) {
      test_ib2 = ib.get();
      break;
    }
  }
  ASSERT_TRUE(test_ib2);

  // Manually dismiss the infobar on browser2.
  infobar_manager2->RemoveInfoBar(test_ib2);

  // It should be removed from browser1 as well (global cascade).
  EXPECT_FALSE(has_test_infobar(infobar_manager1));
  EXPECT_FALSE(has_test_infobar(infobar_manager2));
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       GlobalNoDismissalOnWindowDestruction) {
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

  auto has_test_infobar = [](ContentInfoBarManager* manager) {
    for (const auto& ib : manager->infobars()) {
      if (ib->delegate()->GetIdentifier() == InfoBarDelegate::TEST_INFOBAR) {
        return true;
      }
    }
    return false;
  };

  ASSERT_TRUE(has_test_infobar(infobar_manager1));
  ASSERT_TRUE(has_test_infobar(infobar_manager2));

  // Close browser2. This should destroy its window and its infobar manager,
  // triggering OnInfoBarRemoved due to tab destruction.
  CloseBrowserSynchronously(browser2);

  // The infobar should STILL be present on browser1 (no cascade).
  EXPECT_TRUE(has_test_infobar(infobar_manager1));
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       ShowGloballyRespectsBrowserFilter) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  manager()->Register(BuildGlobalSpecSkippingIncognito());

  manager()->ShowGlobally(InfoBarDelegate::TEST_INFOBAR);

  EXPECT_EQ(1u, InfoBarCountIn(browser()));
  EXPECT_EQ(0u, InfoBarCountIn(incognito_browser));
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       GlobalPropagationRespectsBrowserFilter) {
  manager()->Register(BuildGlobalSpecSkippingIncognito());

  manager()->ShowGlobally(InfoBarDelegate::TEST_INFOBAR);
  EXPECT_EQ(1u, InfoBarCountIn(browser()));

  EXPECT_EQ(0u, InfoBarCountIn(CreateIncognitoBrowser()));
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
  tabs::TabInterface* tab1 = browser()->tab_strip_model()->GetActiveTab();
  manager()->Show(tab1, InfoBarDelegate::TEST_INFOBAR);
  manager()->Show(tab1, InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE);

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

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       TemplateSubstitutionsAndInlineLink) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  content::WebContents* web_contents = tab->GetContents();
  content::WebContents* substitution_contents = nullptr;
  content::WebContents* link_contents = nullptr;
  size_t clicked_index = 0u;

  auto spec =
      InfoBarSpec::Builder(identifier)
          .SetMessageTextTemplate(u"Open $1 to continue")
          .SetSubstitutionsCallback(
              base::BindLambdaForTesting([&](content::WebContents* contents) {
                substitution_contents = contents;
                std::vector<MessageSubstitution> substitutions;
                substitutions.emplace_back(u"the settings page",
                                           /*is_link=*/true,
                                           /*accessible_name=*/std::nullopt);
                return substitutions;
              }))
          .SetInlineLinkCallback(base::BindLambdaForTesting(
              [&](content::WebContents* contents, size_t index,
                  WindowOpenDisposition) {
                link_contents = contents;
                clicked_index = index;
              }))
          .SetScope(InfoBarScope::kTab)
          .Build();

  manager()->Register(std::move(spec));
  manager()->Show(tab, identifier);

  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_EQ(1u, infobar_manager->infobars().size());
  auto* delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);

  // The template and its substitutions should be exposed to the view layer,
  // with the substitutions computed against the WebContents shown in.
  EXPECT_EQ(u"Open $1 to continue", delegate->GetMessageTextTemplate());
  ASSERT_EQ(1u, delegate->GetMessageSubstitutions().size());
  EXPECT_EQ(u"the settings page", delegate->GetMessageSubstitutions()[0].text);
  EXPECT_TRUE(delegate->GetMessageSubstitutions()[0].is_link);
  EXPECT_EQ(web_contents, substitution_contents);

  // GetMessageText() composes the final string for a11y and dupe-checks.
  EXPECT_EQ(u"Open the settings page to continue", delegate->GetMessageText());

  // Clicking the inline link routes to the spec's callback.
  EXPECT_FALSE(delegate->InlineSubstitutionLinkClicked(
      0u, WindowOpenDisposition::CURRENT_TAB));
  EXPECT_EQ(web_contents, link_contents);
  EXPECT_EQ(0u, clicked_index);
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest, DarkModeIcon) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  static constexpr gfx::VectorIcon kLightIcon(nullptr, 0u, "light");
  static constexpr gfx::VectorIcon kDarkIcon(nullptr, 0u, "dark");
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetIcon(kLightIcon)
                  .SetDarkModeIcon(kDarkIcon)
                  .SetScope(InfoBarScope::kTab)
                  .Build();

  manager()->Register(std::move(spec));
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  content::WebContents* web_contents = tab->GetContents();
  manager()->Show(tab, identifier);

  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_EQ(1u, infobar_manager->infobars().size());
  auto* delegate = infobar_manager->infobars()[0]->delegate();

  delegate->set_dark_mode(false);
  EXPECT_EQ(&kLightIcon, &delegate->GetVectorIcon());

  delegate->set_dark_mode(true);
  EXPECT_EQ(&kDarkIcon, &delegate->GetVectorIcon());
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest, CentralizedMetrics) {
  base::HistogramTester histogram_tester;
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec =
      InfoBarSpec::Builder(identifier)
          .SetMessageText(u"Test Message")
          .SetScope(InfoBarScope::kTab)
          .SetLinkNavigationUrl(GURL("about:blank"))
          .AddOkButton(u"OK",
                       base::BindLambdaForTesting([](content::WebContents*) {}))
          .AddCancelButton(u"Cancel", base::BindLambdaForTesting(
                                          [](content::WebContents*) {}))
          .SetDismissAction(
              base::BindLambdaForTesting([](content::WebContents*) {}))
          .Build();

  manager()->Register(std::move(spec));

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  auto* infobar_manager =
      ContentInfoBarManager::FromWebContents(tab->GetContents());

  // 1. Show the infobar and check "Show" metric.
  manager()->Show(tab, identifier);
  ASSERT_EQ(1u, infobar_manager->infobars().size());
  histogram_tester.ExpectUniqueSample("InfoBar.Centralized.Show", identifier,
                                      1);

  auto* delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);

  // 2. Accept and check "Accept" metric.
  delegate->Accept();
  histogram_tester.ExpectUniqueSample("InfoBar.Centralized.Accept", identifier,
                                      1);

  // 3. Cancel and check "Cancel" metric.
  delegate->Cancel();
  histogram_tester.ExpectUniqueSample("InfoBar.Centralized.Cancel", identifier,
                                      1);

  // 4. Dismiss and check "Dismiss" metric.
  delegate->InfoBarDismissed();
  histogram_tester.ExpectUniqueSample("InfoBar.Centralized.Dismiss", identifier,
                                      1);

  // 5. LinkClicked and check "LinkClicked" metric.
  delegate->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  histogram_tester.ExpectUniqueSample("InfoBar.Centralized.LinkClicked",
                                      identifier, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest, TabMovement) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  bool ok_clicked = false;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kTab)
                  .AddOkButton(u"OK", base::BindLambdaForTesting(
                                          [&](content::WebContents* contents) {
                                            EXPECT_TRUE(contents);
                                            ok_clicked = true;
                                          }))
                  .Build();

  manager()->Register(std::move(spec));

  // 1. Show the infobar on the active tab of the first browser.
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  manager()->Show(tab, identifier);

  auto* infobar_manager =
      ContentInfoBarManager::FromWebContents(tab->GetContents());
  ASSERT_EQ(1u, infobar_manager->infobars().size());

  // 2. Create a second browser window.
  Browser* second_browser = CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(second_browser);

  // 3. Move the tab containing the infobar to the second browser.
  std::unique_ptr<tabs::TabModel> tab_model =
      browser()->tab_strip_model()->DetachTabAtForInsertion(0);
  second_browser->tab_strip_model()->AppendTab(std::move(tab_model), true);

  // The infobar manager should still have the infobar on the moved tab.
  auto* new_infobar_manager =
      ContentInfoBarManager::FromWebContents(tab->GetContents());
  ASSERT_EQ(1u, new_infobar_manager->infobars().size());

  auto* delegate =
      new_infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);

  // 4. Interact with the infobar (Accept) and verify it doesn't crash
  // and the callback runs with the correct WebContents.
  EXPECT_TRUE(delegate->Accept());
  EXPECT_TRUE(ok_clicked);
}

}  // namespace infobars
