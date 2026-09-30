// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"

namespace extensions {
namespace {

class IncognitoConnectabilityInteractiveUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  // Rich animations are disabled so that infobars are shown at full height
  // immediately and removed synchronously on close. Otherwise a new infobar
  // can be added at 0px into a container that a pending layout then hides.
  IncognitoConnectabilityInteractiveUiTest()
      : render_mode_resetter_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    if (GetParam()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          infobars::kCentralizedInfoBarFramework,
          {{"MigratedIncognitoConnectability", "true"}});
    } else {
      feature_list_.InitAndDisableFeature(
          infobars::kCentralizedInfoBarFramework);
    }
  }

  ui::ElementContext IncognitoContext(auto* incognito_browser) {
    return views::ElementTrackerViews::GetContextForView(
        BrowserView::GetBrowserViewForBrowser(incognito_browser));
  }

  // Opens an incognito browser with two tabs and queries the same
  // extension/origin pair from both, so a prompt is pending on each tab.
  // Tab 1 is left active. Returns the incognito browser.
  BrowserWindowInterface* QueryFromTwoIncognitoTabs(
      base::test::TestFuture<bool>& first_answer,
      base::test::TestFuture<bool>& second_answer) {
    auto* incognito = CreateIncognitoBrowser();
    auto* connectability =
        IncognitoConnectability::Get(incognito->GetProfile());
    extension_ = ExtensionBuilder("Connectable Extension").Build();

    content::WebContents* first_contents =
        incognito->tab_strip_model()->GetActiveWebContents();
    chrome::AddTabAt(incognito, GURL("about:blank"), -1, true);
    content::WebContents* second_contents =
        incognito->tab_strip_model()->GetActiveWebContents();
    CHECK_NE(first_contents, second_contents);

    connectability->Query(extension_.get(), first_contents,
                          GURL("https://example.com/page"),
                          first_answer.GetCallback());
    connectability->Query(extension_.get(), second_contents,
                          GURL("https://example.com/page"),
                          second_answer.GetCallback());
    return incognito;
  }

 private:
  const gfx::AnimationTestApi::RenderModeResetter render_mode_resetter_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<const Extension> extension_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         IncognitoConnectabilityInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigratedInfobar"
                                             : "LegacyInfobar";
                         });

IN_PROC_BROWSER_TEST_P(IncognitoConnectabilityInteractiveUiTest,
                       AllowAnswersQueryAndPersists) {
  auto* incognito = CreateIncognitoBrowser();
  auto* connectability = IncognitoConnectability::Get(incognito->GetProfile());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Connectable Extension").Build();
  content::WebContents* web_contents =
      incognito->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<bool> first_answer;
  connectability->Query(extension.get(), web_contents,
                        GURL("https://example.com/page"),
                        first_answer.GetCallback());

  RunTestSequenceInContext(
      IncognitoContext(incognito),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      CheckView(ConfirmInfoBar::kInfoBarElementId,
                [](ConfirmInfoBar* infobar) {
                  auto* delegate =
                      infobar->delegate()->AsConfirmInfoBarDelegate();
                  return delegate->GetIdentifier() ==
                             infobars::InfoBarDelegate::
                                 INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE &&
                         delegate->GetMessageText().find(
                             u"Connectable Extension") != std::u16string::npos;
                }),
      PressButton(ConfirmInfoBar::kOkButtonElementId),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  EXPECT_TRUE(first_answer.Get());

  // Allowing persists: the same extension/origin pair is answered without a
  // new prompt.
  base::test::TestFuture<bool> second_answer;
  connectability->Query(extension.get(), web_contents,
                        GURL("https://example.com/other"),
                        second_answer.GetCallback());
  EXPECT_TRUE(second_answer.Get());
  RunTestSequenceInContext(IncognitoContext(incognito),
                           EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_P(IncognitoConnectabilityInteractiveUiTest,
                       DenyAnswersQueryAndPersists) {
  auto* incognito = CreateIncognitoBrowser();
  auto* connectability = IncognitoConnectability::Get(incognito->GetProfile());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Connectable Extension").Build();
  content::WebContents* web_contents =
      incognito->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<bool> first_answer;
  connectability->Query(extension.get(), web_contents,
                        GURL("https://example.com/page"),
                        first_answer.GetCallback());

  RunTestSequenceInContext(IncognitoContext(incognito),
                           WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                           PressButton(ConfirmInfoBar::kCancelButtonElementId),
                           WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  EXPECT_FALSE(first_answer.Get());

  // Denying persists too.
  base::test::TestFuture<bool> second_answer;
  connectability->Query(extension.get(), web_contents,
                        GURL("https://example.com/other"),
                        second_answer.GetCallback());
  EXPECT_FALSE(second_answer.Get());
  RunTestSequenceInContext(IncognitoContext(incognito),
                           EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_P(IncognitoConnectabilityInteractiveUiTest,
                       DismissDeniesQueryButDoesNotPersist) {
  auto* incognito = CreateIncognitoBrowser();
  auto* connectability = IncognitoConnectability::Get(incognito->GetProfile());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Connectable Extension").Build();
  content::WebContents* web_contents =
      incognito->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<bool> first_answer;
  connectability->Query(extension.get(), web_contents,
                        GURL("https://example.com/page"),
                        first_answer.GetCallback());

  RunTestSequenceInContext(IncognitoContext(incognito),
                           WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                           PressButton(ConfirmInfoBar::kDismissButtonElementId),
                           WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  EXPECT_FALSE(first_answer.Get());

  // Dismissing without an explicit choice does not persist: the next query
  // for the same extension/origin prompts again.
  base::test::TestFuture<bool> second_answer;
  connectability->Query(extension.get(), web_contents,
                        GURL("https://example.com/other"),
                        second_answer.GetCallback());
  RunTestSequenceInContext(IncognitoContext(incognito),
                           WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                           PressButton(ConfirmInfoBar::kOkButtonElementId),
                           WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  EXPECT_TRUE(second_answer.Get());
}

IN_PROC_BROWSER_TEST_P(IncognitoConnectabilityInteractiveUiTest,
                       AnsweringOnOneTabClosesTheInfoBarOnAnother) {
  base::test::TestFuture<bool> first_answer;
  base::test::TestFuture<bool> second_answer;
  auto* incognito = QueryFromTwoIncognitoTabs(first_answer, second_answer);

  RunTestSequenceInContext(
      IncognitoContext(incognito),
      // Tab 1 (second_contents) is active and showing its own instance.
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      // Tab 0 (first_contents) has its own instance too.
      SelectTab(kTabStripElementId, 0),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      // Answering definitively on tab 0 closes tab 1's prompt as well.
      PressButton(ConfirmInfoBar::kOkButtonElementId),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
  EXPECT_TRUE(first_answer.Get());
  EXPECT_TRUE(second_answer.Get());
}

IN_PROC_BROWSER_TEST_P(IncognitoConnectabilityInteractiveUiTest,
                       DismissingOnOneTabKeepsTheInfoBarOnAnother) {
  base::test::TestFuture<bool> first_answer;
  base::test::TestFuture<bool> second_answer;
  auto* incognito = QueryFromTwoIncognitoTabs(first_answer, second_answer);

  RunTestSequenceInContext(
      IncognitoContext(incognito),
      // Tab 1 (second_contents) is active and showing its own instance.
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      // Tab 0 (first_contents) has its own instance too.
      SelectTab(kTabStripElementId, 0),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      // Dismissing is not a definitive answer: only tab 0 is answered.
      PressButton(ConfirmInfoBar::kDismissButtonElementId),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  EXPECT_FALSE(first_answer.Get());
  EXPECT_FALSE(second_answer.IsReady());

  // Tab 1's prompt is still open and can still be accepted.
  RunTestSequenceInContext(IncognitoContext(incognito),
                           SelectTab(kTabStripElementId, 1),
                           WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                           PressButton(ConfirmInfoBar::kOkButtonElementId),
                           WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  EXPECT_TRUE(second_answer.Get());
}

}  // namespace
}  // namespace extensions
