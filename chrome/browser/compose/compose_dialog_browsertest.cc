// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_session.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/views/interaction/interaction_test_util_views.h"

using ComposeClientPrefsBrowserTest = InProcessBrowserTest;

namespace compose {

class ComposeSessionBrowserTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list()->InitWithFeatures(
        {compose::features::kEnableCompose,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
    InProcessBrowserTest::SetUp();
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_MAC)
// Mac failures: b/311208586
#define MAYBE_LifetimeOfBubbleWrapper DISABLED_LifetimeOfBubbleWrapper
#else
#define MAYBE_LifetimeOfBubbleWrapper LifetimeOfBubbleWrapper
#endif
IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest,
                       MAYBE_LifetimeOfBubbleWrapper) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));
  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  client->GetComposeEnabling().SetEnabledForTesting();

  // get point of element
  gfx::PointF textarea_center =
      content::GetCenterCoordinatesOfElementWithId(web_contents, "elem1");
  autofill::FormFieldData field_data;
  field_data.bounds = gfx::RectF((textarea_center), gfx::SizeF(1, 1));

  client->ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      field_data, std::nullopt, base::NullCallback());

  // close window right away
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
}

IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest, OpenFeedbackPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));
  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  client->GetComposeEnabling().SetEnabledForTesting();

  // get point of element
  gfx::PointF textarea_center =
      content::GetCenterCoordinatesOfElementWithId(web_contents, "elem1");
  autofill::FormFieldData field_data;
  field_data.bounds = gfx::RectF((textarea_center), gfx::SizeF(1, 1));

  client->ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      field_data, std::nullopt, base::NullCallback());

  client->OpenFeedbackPageForTest("test_id");

  RunTestSequence(
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

// Start ClientPrefsBrowserTest methods.
IN_PROC_BROWSER_TEST_F(ComposeClientPrefsBrowserTest,
                       GetConsentStateFromPrefs) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));
  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  PrefService* prefs = browser()->profile()->GetPrefs();

  // By default both kPageContentCollectionEnabled and
  // kPrefHasAcceptedComposeConsent should be false
  EXPECT_EQ(client->GetConsentStateFromPrefs(),
            compose::mojom::ConsentState::kUnset);

  // Consent enabled but not acknowledged from compose
  prefs->SetBoolean(unified_consent::prefs::kPageContentCollectionEnabled,
                    true);
  EXPECT_EQ(client->GetConsentStateFromPrefs(),
            compose::mojom::ConsentState::kExternalConsented);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Consent enabled and acknowledged from compose
  prefs->SetBoolean(prefs::kPrefHasAcceptedComposeConsent, true);
  EXPECT_EQ(client->GetConsentStateFromPrefs(),
            compose::mojom::ConsentState::kConsented);

  // Consent disabled since being acknowledged from compose
  prefs->SetBoolean(unified_consent::prefs::kPageContentCollectionEnabled,
                    false);
  EXPECT_EQ(client->GetConsentStateFromPrefs(),
            compose::mojom::ConsentState::kUnset);
#endif
}

IN_PROC_BROWSER_TEST_F(ComposeClientPrefsBrowserTest, ApproveConsent) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));
  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  PrefService* prefs = browser()->profile()->GetPrefs();

  // By default both kPageContentCollectionEnabled and
  // kPrefHasAcceptedComposeConsent should be false
  EXPECT_EQ(client->GetConsentStateFromPrefs(),
            compose::mojom::ConsentState::kUnset);

  client->ApproveConsent();
  ASSERT_TRUE(
      prefs->GetBoolean(unified_consent::prefs::kPageContentCollectionEnabled));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ASSERT_TRUE(prefs->GetBoolean(prefs::kPrefHasAcceptedComposeConsent));
#endif
}

IN_PROC_BROWSER_TEST_F(ComposeClientPrefsBrowserTest,
                       AcknowledgeConsentDisclaimer) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));
  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  PrefService* prefs = browser()->profile()->GetPrefs();

  // By default both kPageContentCollectionEnabled and
  // kPrefHasAcceptedComposeConsent should be false
  EXPECT_EQ(client->GetConsentStateFromPrefs(),
            compose::mojom::ConsentState::kUnset);

  client->AcknowledgeConsentDisclaimer();
  ASSERT_FALSE(
      prefs->GetBoolean(unified_consent::prefs::kPageContentCollectionEnabled));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ASSERT_TRUE(prefs->GetBoolean(prefs::kPrefHasAcceptedComposeConsent));
#endif
}

}  // namespace compose
