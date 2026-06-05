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
#include "chrome/browser/contextual_cueing/prefs.h"
#include "chrome/browser/contextual_cueing/test_cue_target.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_model_observer.h"
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
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
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

class ContextualCueingControllerBrowserTestBase : public SigninBrowserTestBase {
 public:
  void SetUp() override {
    InitializeFeatureList();
    SigninBrowserTestBase::SetUp();
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

  void VerifyProactiveCueDecision(
      const ukm::TestAutoSetUkmRecorder& ukm_recorder,
      ContextualCueingDecision expected_decision) {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::ContextualCueing_CueShown::kEntryName);
    ASSERT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntryMetric(
        entries[0].get(),
        ukm::builders::ContextualCueing_CueShown::kProactiveCueDecisionName,
        static_cast<int64_t>(expected_decision));
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

  virtual void InitializeFeatureList() = 0;

 protected:
  raw_ptr<TestCueTarget> cue_target_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;

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

  ui::UserDataFactory::ScopedOverride user_ed_override_;
};

optimization_guide::proto::ContextualCueingResponse MakeCompleteResponse() {
  optimization_guide::proto::ContextualCueingResponse response;
  auto* cue = response.add_contextual_cues();

  // Required UI text
  cue->mutable_anchored_message_cue()->set_action_text("Action text");
  cue->mutable_anchored_message_cue()->set_anchored_message_text(
      "Anchored message text");

  // Fulfillment surface
  cue->mutable_gemini_in_chrome_surface()->set_prompt("Prompt");

  return response;
}

class ContextualCueingControllerBrowserTest
    : public ContextualCueingControllerBrowserTestBase {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualCueingV2,
          {{"ContextualCueingV2DiscardShoppingPdfs", "true"},
           {"ContextualCueingV2TabListVisibility", "always"}}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction});
  }
};

class ContextualCueingControllerTabListNeverTest
    : public ContextualCueingControllerBrowserTestBase {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualCueingV2,
          {{"ContextualCueingV2DiscardShoppingPdfs", "true"},
           {"ContextualCueingV2TabListVisibility", "never"}}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerTabListNeverTest,
                       TabListNotShownWithMultipleTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example.com/1")));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  SessionID background_tab_id =
      sessions::SessionTabHelper::IdForTab(background_contents);

  optimization_guide::proto::ContextualCueingResponse response =
      MakeCompleteResponse();
  auto* cue = response.mutable_contextual_cues(0);
  cue->mutable_anchored_message_cue()->add_tabs_to_show()->set_tab_id(
      background_tab_id.id());

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  ASSERT_TRUE(page_action_controller);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      expandable_content_ = model.GetAnchoredMessageExpandableContent();
    }
    std::optional<page_actions::AnchoredMessageExpandableContent>
        expandable_content_;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  page_action_controller->AddObserver(kActionAnchoredContextualCue,
                                      observation);

  SeedExecutionResult(response);
  SimulateFilterPassed();

  base::HistogramTester histogram_tester;
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  EXPECT_FALSE(observer.expandable_content_.has_value());
}

class ContextualCueingControllerTabListOnlyIfMultipleTest
    : public ContextualCueingControllerBrowserTestBase {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualCueingV2,
          {{"ContextualCueingV2DiscardShoppingPdfs", "true"},
           {"ContextualCueingV2TabListVisibility", "only-if-multiple"}}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerTabListOnlyIfMultipleTest,
                       TabListNotShownWithSingleTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  optimization_guide::proto::ContextualCueingResponse response =
      MakeCompleteResponse();

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  ASSERT_TRUE(page_action_controller);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      expandable_content_ = model.GetAnchoredMessageExpandableContent();
    }
    std::optional<page_actions::AnchoredMessageExpandableContent>
        expandable_content_;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  page_action_controller->AddObserver(kActionAnchoredContextualCue,
                                      observation);

  SeedExecutionResult(response);
  SimulateFilterPassed();

  base::HistogramTester histogram_tester;
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // EXPECT_FALSE(observer.expandable_content_.has_value());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerTabListOnlyIfMultipleTest,
                       TabListShownWithMultipleTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example.com/1")));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  SessionID background_tab_id =
      sessions::SessionTabHelper::IdForTab(background_contents);

  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_contents);

  optimization_guide::proto::ContextualCueingResponse response =
      MakeCompleteResponse();
  auto* cue = response.mutable_contextual_cues(0);
  auto* tab1 = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  tab1->set_tab_id(background_tab_id.id());
  tab1->set_url("https://www.example.com/1");
  auto* tab2 = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  tab2->set_tab_id(active_tab_id.id());
  tab2->set_url("https://www.activetab.com/abc");

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  ASSERT_TRUE(page_action_controller);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      expandable_content_ = model.GetAnchoredMessageExpandableContent();
    }
    std::optional<page_actions::AnchoredMessageExpandableContent>
        expandable_content_;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  page_action_controller->AddObserver(kActionAnchoredContextualCue,
                                      observation);

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SeedExecutionResult(response);
  SimulateFilterPassed();

  base::HistogramTester histogram_tester;
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueShown::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0].get(),
      ukm::builders::ContextualCueing_CueShown::kMatchedTabCountName,
      ukm::GetExponentialBucketMin(2, 1.5));

  EXPECT_TRUE(observer.expandable_content_.has_value());
  EXPECT_EQ(observer.expandable_content_->expand_button_tooltip,
            u"Show tab sharing details. Sharing 2 tabs from www.activetab.com, "
            u"www.example.com");
}

IN_PROC_BROWSER_TEST_F(
    ContextualCueingControllerBrowserTest,
    ShouldNotRecordDecisionIfReturnedCategoryClassificationNotForActiveTab) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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

  histogram_tester.ExpectTotalCount("ContextualCueing.V2.Decision", 0);
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::ContextualCueing_CueShown::kEntryName)
                  .empty());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       FailedCategoryClassification) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.example.com/abc")));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  VerifyProactiveCueDecision(
      ukm_recorder, ContextualCueingDecision::kFailedCategoryClassification);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PassesFilterButModelExecutionFailed) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.example.com/abc")));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  VerifyProactiveCueDecision(
      ukm_recorder,
      ContextualCueingDecision::kModelExecutionResponseFailedToParse);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PassesFilterAndModelExecutionSucceeded) {
  browser()->profile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicDefaultTabContextEnabled, true);

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

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  auto response = MakeCompleteResponse();
  auto* cue = response.mutable_contextual_cues(0);
  cue->set_suggested_cuj("TestCUJ");

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Add a valid tab to the response.
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_web_contents);
  auto* valid_tab = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  valid_tab->set_tab_id(active_tab_id.id());
  valid_tab->set_url("https://www.example.com/abc");

  // Add an invalid tab to the response.
  auto* invalid_tab = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  invalid_tab->set_tab_id(9999);

  SeedExecutionResult(response);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      content_ = model.GetAnchoredMessageExpandableContent();
    }
    std::optional<page_actions::AnchoredMessageExpandableContent> content_;
  };

  TestObserver model_observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&model_observer);
  GetPageActionController()->AddObserver(kActionAnchoredContextualCue,
                                         observation);

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

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueShown",
                                      base::HashMetricName("TestCUJ"), 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueShown::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0].get();

  ukm::SourceId expected_source_id =
      active_web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  EXPECT_EQ(expected_source_id, entry->source_id);
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_CueShown::kSuggestedCujCategoryName,
      base::HashMetricName("TestCUJ"));

  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_CueShown::kProactiveCueDecisionName,
      static_cast<int64_t>(ContextualCueingDecision::kSuccess));

  // One valid tab in the response.
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::ContextualCueing_CueShown::kMatchedTabCountName,
      ukm::GetExponentialBucketMin(1, 1.5));

  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::ContextualCueing_CueShown::kMissingTabCountName,
      ukm::GetExponentialBucketMin(1, 1.5));

  const int64_t* latency_value = ukm_recorder.GetEntryMetric(
      entry, ukm::builders::ContextualCueing_CueShown::
                 kProactiveCueLatencyAfterPageLoadName);
  ASSERT_TRUE(latency_value);
  EXPECT_GE(*latency_value, 0);

  // Verify expandable content.
  EXPECT_TRUE(model_observer.content_.has_value());
  EXPECT_EQ(model_observer.content_->items.size(), 1u);
  EXPECT_FALSE(model_observer.content_->items[0].text.empty());
  EXPECT_EQ(model_observer.content_->expand_button_tooltip,
            u"Show tab sharing details. Sharing 1 tab from www.example.com");
  // No favicon provided, so we should have logged it as missing.
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.MissingFaviconCount",
                                      1, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NoAnchoredMessageCueInResponse) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->clear_anchored_message_cue();
  SeedExecutionResult(std::move(response));

  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kMissingAnchoredMessageText, 1);
  VerifyProactiveCueDecision(
      ukm_recorder, ContextualCueingDecision::kMissingAnchoredMessageText);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       UnknownFulfillmentSurface) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->clear_gemini_in_chrome_surface();
  SeedExecutionResult(std::move(response));

  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kUnknownFulfillmentSurface, 1);
  VerifyProactiveCueDecision(
      ukm_recorder, ContextualCueingDecision::kUnknownFulfillmentSurface);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, Ineligible) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kNoEligibleCueSurfaces);
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
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

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

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueInteraction::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0].get();
  const int64_t* duration_value = ukm_recorder.GetEntryMetric(
      entry, ukm::builders::ContextualCueing_CueInteraction::
                 kProactiveCueShownDurationMsName);
  ASSERT_TRUE(duration_value);
  EXPECT_GE(*duration_value, 0);

  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_CueInteraction::
          kProactiveCueInteractionName,
      static_cast<int64_t>(ContextualCueingInteraction::kCueClicked));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       ShowCueAndClickAsIcon) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_FALSE(cue_target_->HasClickData());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

  // Initially the anchored message is shown.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Hide the anchored message so the page action collapses to an icon.
  page_action_controller->HideAnchoredMessage(kActionAnchoredContextualCue);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !observer.GetCurrentPageActionState().anchored_message_showing;
  }));
  EXPECT_FALSE(observer.GetCurrentPageActionState().chip_showing);
  EXPECT_TRUE(observer.GetCurrentPageActionState().showing);

  // Invoke/click the page action. It should show the anchored message instead
  // of calling Click handler.
  auto* action =
      actions::ActionManager::Get().FindAction(kActionAnchoredContextualCue);
  ASSERT_TRUE(action);
  action->InvokeAction();

  // Target click handler was not invoked.
  EXPECT_FALSE(cue_target_->HasClickData());

  // Anchored message is showing again.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NoLongerActiveTabAfterResponse) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
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
  VerifyProactiveCueDecision(
      ukm_recorder,
      ContextualCueingDecision::kNoLongerActiveTabAfterModelExecution);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       FeaturePromoActive) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
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
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kFeaturePromoActive);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, HistorySyncOff) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EnableHistorySync(false);
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kHistorySyncOff,
                                      1);
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kHistorySyncOff);
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
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    // Navigate to a valid URL.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("https://www.activetab.com/abc")));

    SeedExecutionResult(MakeCompleteResponse());
    SimulateFilterPassed();

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester, "ContextualCueing.V2.Decision", 1);
    histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                        ContextualCueingDecision::kSuccess, 1);
    VerifyProactiveCueDecision(ukm_recorder,
                               ContextualCueingDecision::kSuccess);
  }

  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

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
    VerifyProactiveCueDecision(
        ukm_recorder,
        ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue);
  }
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NonHttpUrlNotEligible) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kUrlNotEligible);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       GoogleSearchUrlNotEligible) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kUrlNotEligible);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       OtherSearchEngineUrlNotEligible) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kUrlNotEligible);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HomePageNotEligible) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

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
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kUrlNotEligible);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueNotShowingBecauseSidePanelOpen) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
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
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kSidePanelShowing);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueHidesWhenSidePanelOpened) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

  // Initially, the contextual cue anchored message is shown on the screen.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Open the side panel (we use Bookmarks here as a standard global entry).
  auto* side_panel_ui = SidePanelUIProvider::From(browser());
  ASSERT_TRUE(side_panel_ui);
  side_panel_ui->Show(SidePanelEntryId::kBookmarks);

  // Verify that our observer successfully intercepted the open event
  // and hid the contextual cue dynamically.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().showing; }));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueNotShowingBecauseInfobarVisible) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  // Add an infobar to the active tab.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager->AddInfoBar(
      ConfirmInfoBar::Create(std::make_unique<TestInfoBarDelegate>()));
  ASSERT_FALSE(infobar_manager->infobars().empty());

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kInfobarVisible,
                                      1);
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kInfobarVisible);
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
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kUserOptedOut,
                                      1);
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kUserOptedOut);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       DisabledByEnterprisePolicy) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      optimization_guide::prefs::kChromeSuggestionsSettings,
      static_cast<int>(
          contextual_cueing::ChromeSuggestionsSettingsValue::kDisabled));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kDisabledByEnterprisePolicy, 1);
  VerifyProactiveCueDecision(
      ukm_recorder, ContextualCueingDecision::kDisabledByEnterprisePolicy);
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
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // 2. Mock the server response and inject a fake CUJ string
  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->set_suggested_cuj("test_cuj_string");
  SeedExecutionResult(std::move(response));

  // 3. Trigger the cue execution flow
  SimulateFilterPassed();

  // 4. Wait for the flow to successfully finish
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 5. Confirm flow was completed successfully
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

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
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // 2. Mock the server response and inject a fake CUJ string
  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->set_suggested_cuj("test_cuj_string");
  SeedExecutionResult(std::move(response));

  // 3. Trigger the cue execution flow
  SimulateFilterPassed();

  // 4. Wait for the flow to successfully finish
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 5. Confirm flow was completed successfully
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

  // 6. Simulate user clicking the cue
  auto* action =
      actions::ActionManager::Get().FindAction(kActionAnchoredContextualCue);
  ASSERT_TRUE(action);
  action->InvokeAction();

  // 7. Verify that the interaction was logged with the hashed CUJ!
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Clicked",
      base::HashMetricName("test_cuj_string"), 1);

  // Verify UKM metric.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueInteraction::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_CueInteraction::
          kProactiveCueInteractionName,
      static_cast<int64_t>(ContextualCueingInteraction::kCueClicked));
}

#if BUILDFLAG(ENABLE_PDF)

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PdfEduOnlyIsSupported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), pdf_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  EXPECT_EQ(active_web_contents->GetContentsMimeType(), "application/pdf");

  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          pdf_url),
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
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PdfShoppingOnlyIsNotSupported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), pdf_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          pdf_url),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.2),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.9),
          }));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kFailedCategoryClassification, 1);
  VerifyProactiveCueDecision(
      ukm_recorder, ContextualCueingDecision::kFailedCategoryClassification);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       PdfEduAndShoppingIsNotSupported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), pdf_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          pdf_url),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.9),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.9),
          }));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kFailedCategoryClassification, 1);
  VerifyProactiveCueDecision(
      ukm_recorder, ContextualCueingDecision::kFailedCategoryClassification);
}

#endif  // BUILDFLAG(ENABLE_PDF)

class ContextualCueingControllerBrowserTestWithAgeRestriction
    : public ContextualCueingControllerBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {kContextualCueingV2, kContextualCueingV2EnforceAgeRestriction},
        /*disabled_features=*/{});
  }

  void SetUserRestriction(bool is_restricted) {
    auto account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(!is_restricted);
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTestWithAgeRestriction,
                       AgeRestrictionEnforced) {
  SetUserRestriction(/*is_restricted=*/true);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kAgeRestrictionEnforced, 1);
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kAgeRestrictionEnforced);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTestWithAgeRestriction,
                       AgeRestrictionPasses) {
  SetUserRestriction(/*is_restricted=*/false);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

#if BUILDFLAG(ENABLE_PDF)

class ContextualCueingControllerDoNotDiscardShoppingPdfsTest
    : public ContextualCueingControllerBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualCueingV2,
          {{"ContextualCueingV2DiscardShoppingPdfs", "false"}}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerDoNotDiscardShoppingPdfsTest,
                       PdfEduOnlyIsSupported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), pdf_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  EXPECT_EQ(active_web_contents->GetContentsMimeType(), "application/pdf");

  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          pdf_url),
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
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerDoNotDiscardShoppingPdfsTest,
                       PdfShoppingOnlyIsSupported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), pdf_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          pdf_url),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.2),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.9),
          }));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerDoNotDiscardShoppingPdfsTest,
                       PdfEduAndShoppingIsNotSupported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), pdf_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  contextual_cueing_controller()->OnPageContentAnnotated(
      page_content_annotations::HistoryVisit(
          active_web_contents->GetController()
              .GetLastCommittedEntry()
              ->GetTimestamp(),
          pdf_url),
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kEducation, 0.9),
              page_content_annotations::Category(
                  page_content_annotations::CategoryType::kShopping, 0.9),
          }));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

#endif

}  // namespace
}  // namespace contextual_cueing
