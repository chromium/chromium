// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/metrics/user_action_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/compose/compose_session.h"
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
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace compose {

class ComposeSessionBrowserTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    scoped_compose_enabled_ = ComposeEnabling::ScopedEnableComposeForTesting();
    feature_list()->InitWithExistingFeatures(
        {compose::features::kEnableCompose,
         optimization_guide::features::kOptimizationGuideModelExecution,
         feature_engagement::kIPHComposeMSBBSettingsFeature});
    InteractiveBrowserTest::SetUp();
  }

  void TearDown() override {}

  feature_engagement::test::ScopedIphFeatureList* feature_list() {
    return &feature_list_;
  }

 protected:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
  ComposeEnabling::ScopedOverride scoped_compose_enabled_;
};

IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest, LifetimeOfBubbleWrapper) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));

  // get point of element
  gfx::PointF textarea_center =
      content::GetCenterCoordinatesOfElementWithId(web_contents, "elem1");
  autofill::FormFieldData field_data;
  field_data.set_bounds(gfx::RectF((textarea_center), gfx::SizeF(1, 1)));

  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  client->ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      field_data, std::nullopt, base::NullCallback());

  // close window right away
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
}

IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest, OpenFeedbackPage) {
  // Feedback page can only be opened from a dialog state where MSSB is enabled.
  // TODO(b/316601302): Without directly setting the MSBB pref value this test
  // is flaky on Linux MSan builders. This requires further investigation, but
  // the MSBB dialog state is not on the feedback page testing path so the
  // current state still satisfies the test requirement.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));

  // get point of element
  gfx::PointF textarea_center =
      content::GetCenterCoordinatesOfElementWithId(web_contents, "elem1");
  autofill::FormFieldData field_data;
  field_data.set_bounds(gfx::RectF((textarea_center), gfx::SizeF(1, 1)));

  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  client->ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      field_data, std::nullopt, base::NullCallback());

  client->OpenFeedbackPageForTest("test_id");

  RunTestSequence(
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest,
                       TestDialogClosedAfterPageScrolled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));
  auto* client = ChromeComposeClient::FromWebContents(web_contents);

  // get point of element
  gfx::PointF textarea_center =
      content::GetCenterCoordinatesOfElementWithId(web_contents, "elem1");
  autofill::FormFieldData field_data;
  field_data.set_bounds(gfx::RectF((textarea_center), gfx::SizeF(1, 1)));

  client->ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      field_data, std::nullopt, base::NullCallback());

  EXPECT_TRUE(client->IsDialogShowing());

  // Scroll on page
  blink::WebGestureEvent event;
  event.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
  client->DidGetUserInteraction(event);

  EXPECT_FALSE(client->IsDialogShowing());
}

IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest, SettingsLaunchedTest) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));

  // get point of element
  gfx::PointF textarea_center =
      content::GetCenterCoordinatesOfElementWithId(web_contents, "elem1");
  autofill::FormFieldData field_data;
  field_data.set_bounds(gfx::RectF((textarea_center), gfx::SizeF(1, 1)));

  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  client->ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      field_data, std::nullopt, base::NullCallback());

  client->OpenComposeSettings();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.SessionPaused.MSBBSettingsShown"));

  int tab_index = browser()->tab_strip_model()->count() - 1;

  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  browser()->tab_strip_model()->CloseWebContentsAt(
      tab_index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  destroyed_watcher.Wait();

  EXPECT_EQ(embedded_test_server()->GetURL("/compose/test2.html"),
            web_contents->GetVisibleURL());
  EXPECT_TRUE(client->IsDialogShowing());
}

IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest,
                       PractiveNudgeSettingsLaunchedTest) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));

  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  ASSERT_NE(nullptr, client);

  autofill::FormFieldData field_data;

  client->OpenProactiveNudgeSettings();

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  histogram_tester.ExpectUniqueSample(
      compose::kComposeProactiveNudgeCtr,
      compose::ComposeNudgeCtrEvent::kOpenSettings, 1);
}

IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest,
                       SelectionNudgeSettingsLaunchedTest) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));

  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  ASSERT_NE(nullptr, client);

  autofill::FormFieldData field_data;
  autofill::FormData form_data;

  // Set the most recent nudge to the selection nudge.
  client->ShowProactiveNudge(form_data.global_id(), field_data.global_id(),
                             compose::ComposeEntryPoint::kSelectionNudge);
  client->OpenProactiveNudgeSettings();

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  histogram_tester.ExpectUniqueSample(
      compose::kComposeSelectionNudgeCtr,
      compose::ComposeNudgeCtrEvent::kOpenSettings, 1);
}

}  // namespace compose
