// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_cueing/test_cue_target.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"

namespace contextual_cueing {
namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

using ::testing::Return;

class TestInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  TestInfoBarDelegate() = default;
  ~TestInfoBarDelegate() override = default;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return infobars::InfoBarDelegate::TEST_INFOBAR;
  }
  std::u16string GetMessageText() const override { return u"Test InfoBar"; }
};

class ContextualCueingControllerBrowserTest : public SigninBrowserTestBase {
 public:
  ContextualCueingControllerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kContextualCueingV2);
  }

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();

    auto test_cue_target = std::make_unique<TestCueTarget>();
    cue_target_ = test_cue_target.get();
    contextual_cueing_controller()->RegisterCueTarget(
        CueTargetType::kGlic, std::move(test_cue_target));

    // Enable history sync by default.
    EnableHistorySync(true);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SigninBrowserTestBase::SetUpInProcessBrowserTestFixture();

    // Override the creation of BrowserUserEducationInterface to
    // use the mock.
    user_ed_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](BrowserWindowInterface& window) {
                  return std::make_unique<
                      testing::NiceMock<MockBrowserUserEducationInterface>>(
                      &window);
                }));
  }

  void TearDownOnMainThread() override {
    cue_target_ = nullptr;
    SigninBrowserTestBase::TearDownOnMainThread();
  }

  void EnableHistorySync(bool enabled) {
    GetTestSyncService()->SetSignedIn(signin::ConsentLevel::kSignin);
    GetTestSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, enabled);
  }

  ContextualCueingController* contextual_cueing_controller() {
    return browser()->GetFeatures().contextual_cueing_controller();
  }

  MockBrowserUserEducationInterface* mock_user_education_interface() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser()));
  }

  void SeedExecutionResult(
      optimization_guide::OptimizationGuideModelExecutionResult result) {
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::kContextualCueing,
            std::move(result));
  }

  void SeedExecutionResult(
      optimization_guide::proto::ContextualCueingResponse response) {
    optimization_guide::proto::Any response_any;
    response.SerializeToString(response_any.mutable_value());
    response_any.set_type_url(
        base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
    SeedExecutionResult(
        optimization_guide::OptimizationGuideModelExecutionResult(
            response_any, /*execution_info=*/nullptr));
  }

  void SimulateFilterPassed(
      const GURL& url = GURL("https://www.activetab.com/abc")) {
    content::WebContents* active_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(active_web_contents);
    contextual_cueing_controller()->OnPageContentAnnotated(
        page_content_annotations::HistoryVisit(
            active_web_contents->GetController()
                .GetLastCommittedEntry()
                ->GetTimestamp(),
            url),
        page_content_annotations::PageContentAnnotationsResult::
            CreateCategoryResults({
                page_content_annotations::Category(
                    page_content_annotations::CategoryType::kEducation, 0.9),
                page_content_annotations::Category(
                    page_content_annotations::CategoryType::kShopping, 0.2),
            }));
  }

  page_actions::PageActionController* GetPageActionController() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->page_action_controller();
  }

 protected:
  raw_ptr<TestCueTarget> cue_target_ = nullptr;

 private:
  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->profile()));
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UserDataFactory::ScopedOverride user_ed_override_;
};

optimization_guide::proto::ContextualCueingResponse MakeCompleteResponse() {
  optimization_guide::proto::ContextualCueingResponse response;

  // Required UI text
  response.mutable_anchored_message_cue()->set_action_text("Action text");
  response.mutable_anchored_message_cue()->set_anchored_message_text(
      "Anchored message text");

  // Fulfillment surface
  response.mutable_gemini_in_chrome_surface()->set_prompt("Prompt");

  return response;
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NoLongerActiveTabAfterCategoryClassification) {
  base::HistogramTester histogram_tester;

  // Have browser navigate to a valid URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.example.com/abc")));

  // Navigate to different page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.other.com")));

  // URL won't match whatever navigated since it does not match the active tab.
  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          base::Time::Now(), GURL("https://www.example.com/abc")),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.9),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.4),
          }));

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kNoLongerActiveTabAfterCategoryClassification,
      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       FailedCategoryClassification) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.example.com/abc")));

  base::HistogramTester histogram_tester;

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          GURL("https://www.example.com/abc")),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.4),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.2),
          }));

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kFailedCategoryClassification, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PassesFilterButModelExecutionFailed) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.example.com/abc")));

  base::HistogramTester histogram_tester;

  // Seed empty execution result.
  optimization_guide::OptimizationGuideModelExecutionResult result(
      optimization_guide::proto::Any(), /*execution_info=*/nullptr);
  SeedExecutionResult(std::move(result));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          GURL("https://www.example.com/abc")),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.9),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.2),
          }));
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kModelExecutionResponseFailedToParse, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PassesFilterAndModelExecutionSucceeded) {
  // Navigate current Chrome tab to a valid URL (and will be in the background
  // in final state).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.someurl.com/abc")));

  // Create a new tab that is specifically a URL that would normally be skipped
  // (will be in the background in final state).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://settings"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Navigate to a new eligible tab to be in the foreground (current active
  // tab).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;

  SeedExecutionResult(MakeCompleteResponse());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          GURL("https://www.example.com/abc")),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.9),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.2),
          }));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // There are three total tabs (one is active, one is valid as a background
  // tab, and the other is a new tab). Active and non HTTP/HTTPS tabs are
  // skipped.
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.NumRequestedBackgroundTabs", 1, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NoAnchoredMessageCueInResponse) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  auto response = MakeCompleteResponse();
  response.clear_anchored_message_cue();
  SeedExecutionResult(std::move(response));

  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kMissingAnchoredMessageText, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       UnknownFulfillmentSurface) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  auto response = MakeCompleteResponse();
  response.clear_gemini_in_chrome_surface();
  SeedExecutionResult(std::move(response));

  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kUnknownFulfillmentSurface, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, Ineligible) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  cue_target_->eligible = false;
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kNoEligibleCueSurfaces, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, ShowCueAndClick) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_FALSE(cue_target_->HasClickData());

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;

  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  auto* action =
      actions::ActionManager::Get().FindAction(kActionAnchoredContextualCue);
  ASSERT_TRUE(action);
  action->InvokeAction();

  ASSERT_TRUE(cue_target_->HasClickData());
  EXPECT_EQ("Prompt",
            std::get<GlicCueActionData>(cue_target_->click_data).prompt);
  EXPECT_FALSE(observer.GetCurrentPageActionState().showing);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueInteraction",
                                      ContextualCueingInteraction::kCueClicked,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NoLongerActiveTabAfterResponse) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  // Open new tab in foreground right away.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kNoLongerActiveTabAfterModelExecution, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       FeaturePromoActive) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  // Simulate feature promo showing.
  EXPECT_CALL(*mock_user_education_interface(), IsAnyFeaturePromoActive())
      .WillOnce(Return(true));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kFeaturePromoActive, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, HistorySyncOff) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  EnableHistorySync(false);
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kHistorySyncOff,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       OnlySendsTopMaxBackgroundTabs) {
  // Create 15 tabs.
  for (int i = 0; i < kMaxNumBackgroundTabs.Get(); ++i) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(base::StringPrintf("https://www.google.com/%d", i)),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 15 background tabs + 1 active tab.
  // We expect only the max allowed background tabs to be requested.
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.NumRequestedBackgroundTabs",
      kMaxNumBackgroundTabs.Get(), 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NotEnoughPageLoadsSinceLastCue) {
  {
    base::HistogramTester histogram_tester;

    // Navigate to a valid URL.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("https://www.activetab.com/abc")));

    SeedExecutionResult(MakeCompleteResponse());
    SimulateFilterPassed();

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester, "ContextualCueing.V2.Decision", 1);
    histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                        ContextualCueingDecision::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;

    // Simulate a new page load.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("https://www.activetab.com/abc")));
    SimulateFilterPassed();

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester, "ContextualCueing.V2.Decision", 1);

    // Should not be shown.
    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.V2.Decision",
        ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue, 1);
  }
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NonHttpUrlNotEligible) {
  base::HistogramTester histogram_tester;

  // Simulate a new page load.
  GURL non_http_url("chrome://settings");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_http_url));
  SimulateFilterPassed(non_http_url);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // Should not be shown.
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kUrlNotEligible,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       GoogleSearchUrlNotEligible) {
  base::HistogramTester histogram_tester;

  // Simulate a new page load.
  GURL search_url("https://www.google.com/search?q=test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));
  SimulateFilterPassed(search_url);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // Should not be shown.
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kUrlNotEligible,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       OtherSearchEngineUrlNotEligible) {
  base::HistogramTester histogram_tester;

  // Simulate a new page load.
  GURL search_url("https://duckduckgo.com/?q=test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));
  SimulateFilterPassed(search_url);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // Should not be shown.
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kUrlNotEligible,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HomePageNotEligible) {
  base::HistogramTester histogram_tester;

  // Simulate a new page load.
  GURL homepage_url("https://activetab.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), homepage_url));
  SimulateFilterPassed(homepage_url);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // Should not be shown.
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kUrlNotEligible,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueNotShowingBecauseSidePanelOpen) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());

  // Open side panel.
  auto* side_panel_ui = SidePanelUIProvider::From(browser());
  ASSERT_TRUE(side_panel_ui);
  side_panel_ui->Show(SidePanelEntryId::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kBookmarks));
  }));

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kSidePanelShowing, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueNotShowingBecauseInfobarVisible) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());

  // Add an infobar to the active tab.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager->AddInfoBar(std::make_unique<ConfirmInfoBar>(
      std::make_unique<TestInfoBarDelegate>()));
  ASSERT_FALSE(infobar_manager->infobars().empty());

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kInfobarVisible,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, UserOptedOut) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      static_cast<int>(
          optimization_guide::prefs::FeatureOptInState::kDisabled));

  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kUserOptedOut,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       DisabledByEnterprisePolicy) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      optimization_guide::prefs::kContextualCueingEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable));

  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kDisabledByEnterprisePolicy, 1);
}

// TODO(crbug.com/503910711): Add a test for hiding on navigation

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       RecordsShownCueCUJHistogram) {
  // 1. Navigate to a valid eligible URL
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;

  // 2. Mock the server response and inject a fake CUJ string
  auto response = MakeCompleteResponse();
  response.set_suggested_cuj("test_cuj_string");
  SeedExecutionResult(std::move(response));

  // 3. Trigger the cue execution flow
  SimulateFilterPassed();

  // 4. Wait for the flow to successfully finish
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 5. Confirm flow was completed successfully
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // 6. Verify your new histogram!
  histogram_tester.ExpectUniqueSample("ContextualCueing.ShownCueCUJ",
                                      base::HashMetricName("test_cuj_string"),
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       RecordsCueInteractionWithCUJHistogram) {
  // 1. Navigate to a valid eligible URL
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;

  // 2. Mock the server response and inject a fake CUJ string
  auto response = MakeCompleteResponse();
  response.set_suggested_cuj("test_cuj_string");
  SeedExecutionResult(std::move(response));

  // 3. Trigger the cue execution flow
  SimulateFilterPassed();

  // 4. Wait for the flow to successfully finish
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 5. Confirm flow was completed successfully
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // 6. Simulate user clicking the cue
  auto* action =
      actions::ActionManager::Get().FindAction(kActionAnchoredContextualCue);
  ASSERT_TRUE(action);
  action->InvokeAction();

  // 7. Verify that the interaction was logged with the hashed CUJ!
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Clicked",
      base::HashMetricName("test_cuj_string"), 1);
}

}  // namespace
}  // namespace contextual_cueing
