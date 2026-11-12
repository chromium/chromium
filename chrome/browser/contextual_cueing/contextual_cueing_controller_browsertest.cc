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
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_cueing/prefs.h"
#include "chrome/browser/contextual_cueing/test_cue_target.h"
#include "chrome/browser/contextual_search/contextual_search_cue_tab_state.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/suggestions/glic_cue_tab_state.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/confirm_infobar_creation_allowlist.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/private_insights/private_insights_service_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/contextual_cueing/contextual_cueing_enums.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/sync/history_backend_for_sync.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/page_content_annotations/core/page_category_classifier_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/private_insights/events/contextual_cue_log_event.pb.h"
#include "components/private_insights/private_insights_features.h"
#include "components/private_insights/private_insights_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/sync/test/test_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

namespace contextual_cueing {
namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class MockPrivateInsightsService
    : public private_insights::PrivateInsightsService {
 public:
  MockPrivateInsightsService(
      PrefService* local_state,
      const base::FilePath& profile_dir,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : private_insights::PrivateInsightsService(
            local_state,
            profile_dir,
            std::move(url_loader_factory)) {}
  MOCK_METHOD(void,
              LogContextualCueEvent,
              (private_insights::events::ContextualCueLogEvent event),
              (override));
};

std::unique_ptr<KeyedService> CreateMockPrivateInsightsService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockPrivateInsightsService>>(
      g_browser_process->local_state(), context->GetPath(),
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
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

class FakePageCategoryClassifierBridge
    : public page_content_annotations::PageCategoryClassifierBridge {
 public:
  void SetDemandActive(bool active) override { demand_active_ = active; }
  bool demand_active() const { return demand_active_; }

 private:
  bool demand_active_ = false;
};

class ContextualCueingControllerBrowserTestBase : public SigninBrowserTestBase,
                                                  public TabStripModelObserver {
 public:
  void SetUp() override {
    InitializeFeatureList();
    SigninBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();

    browser()->GetTabStripModel()->AddObserver(this);

    RegisterTestCueTargetForTab(browser()->GetActiveTabInterface());

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
    browser()->GetTabStripModel()->RemoveObserver(this);
    SigninBrowserTestBase::TearDownOnMainThread();
  }

  void EnableHistorySync(bool enabled) {
    GetTestSyncService()->SetSignedIn(signin::ConsentLevel::kSignin);
    GetTestSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, enabled);
    GetTestSyncService()->FireStateChanged();
  }

  FakePageCategoryClassifierBridge* AttachFakeCategoryClassifierBridge() {
    auto bridge = std::make_unique<FakePageCategoryClassifierBridge>();
    FakePageCategoryClassifierBridge* bridge_ptr = bridge.get();
    PageContentAnnotationsServiceFactory::GetForProfile(browser()->GetProfile())
        ->SetPageCategoryClassifierBridge(std::move(bridge));
    return bridge_ptr;
  }

  ContextualCueingController* contextual_cueing_controller() {
    return ContextualCueingController::From(browser()->GetActiveTabInterface());
  }

  TestCueTarget* cue_target() {
    return static_cast<TestCueTarget*>(
        contextual_cueing_controller()->GetTarget(CueTargetType::kGlic));
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents : change.GetInsert()->contents) {
        RegisterTestCueTargetForTab(contents.tab);
      }
    }
  }

  void RegisterTestCueTargetForTab(tabs::TabInterface* tab) {
    auto test_cue_target = std::make_unique<TestCueTarget>();
    ContextualCueingController::From(tab)->RegisterCueTarget(
        CueTargetType::kGlic, std::move(test_cue_target));
  }

  MockBrowserUserEducationInterface* mock_user_education_interface() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser()));
  }

  void SeedExecutionResult(
      optimization_guide::OptimizationGuideModelExecutionResult result) {
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->GetProfile())
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
        browser()->GetTabStripModel()->GetActiveWebContents();
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
    return page_actions::PageActionController::From(
        browser()->GetActiveTabInterface());
  }

  virtual void InitializeFeatureList() = 0;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->GetProfile()));
  }

  MockPrivateInsightsService* GetMockPrivateInsightsService() {
    return static_cast<MockPrivateInsightsService*>(
        private_insights::PrivateInsightsServiceFactory::GetForProfile(
            browser()->GetProfile()));
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
    private_insights::PrivateInsightsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating(&CreateMockPrivateInsightsService));
  }

 private:
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
           {"ContextualCueingV2TabListVisibility", "always"},
           {"ContextualCueingV2EnablePrivateInsightsLogging", "true"}}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction,
                               kContextualCueingV2MultiSource});
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
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction,
                               kContextualCueingV2MultiSource});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerTabListNeverTest,
                       TabListNotShownWithMultipleTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example.com/1")));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* background_contents =
      browser()->GetTabStripModel()->GetWebContentsAt(0);
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
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction,
                               kContextualCueingV2MultiSource});
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* background_contents =
      browser()->GetTabStripModel()->GetWebContentsAt(0);
  SessionID background_tab_id =
      sessions::SessionTabHelper::IdForTab(background_contents);

  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
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
  EXPECT_EQ(observer.expandable_content_->expand_button_accessible_name,
            u"Tab sharing details. Sharing 2 tabs from www.activetab.com, "
            u"www.example.com");
}

IN_PROC_BROWSER_TEST_F(
    ContextualCueingControllerTabListOnlyIfMultipleTest,
    RemoveTabIncludedInActiveMultiTabCueStopsShowingCueOnActiveTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example.com/1")));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* background_contents =
      browser()->GetTabStripModel()->GetWebContentsAt(0);
  SessionID background_tab_id =
      sessions::SessionTabHelper::IdForTab(background_contents);

  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_contents);

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  optimization_guide::proto::ContextualCueingResponse response =
      MakeCompleteResponse();
  auto* cue = response.mutable_contextual_cues(0);
  auto* tab1 = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  tab1->set_tab_id(background_tab_id.id());
  tab1->set_url("https://www.example.com/1");
  auto* tab2 = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  tab2->set_tab_id(active_tab_id.id());
  tab2->set_url("https://www.activetab.com/abc");

  SeedExecutionResult(response);
  SimulateFilterPassed();

  base::HistogramTester histogram_tester;
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // The contextual cue anchored message should be shown on the active tab.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Close the background tab.
  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  // The contextual cue anchored message should not be shown on the active
  // tab.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !observer.GetCurrentPageActionState().anchored_message_showing;
  }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualCueingControllerTabListOnlyIfMultipleTest,
    UrlChangeOnBackgroundTabForMultiTabCueStopsShowingCueOnActiveTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example.com/1")));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* background_contents =
      browser()->GetTabStripModel()->GetWebContentsAt(0);
  SessionID background_tab_id =
      sessions::SessionTabHelper::IdForTab(background_contents);

  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_contents);

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  optimization_guide::proto::ContextualCueingResponse response =
      MakeCompleteResponse();
  auto* cue = response.mutable_contextual_cues(0);
  auto* tab1 = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  tab1->set_tab_id(background_tab_id.id());
  tab1->set_url("https://www.example.com/1");
  auto* tab2 = cue->mutable_anchored_message_cue()->add_tabs_to_show();
  tab2->set_tab_id(active_tab_id.id());
  tab2->set_url("https://www.activetab.com/abc");

  SeedExecutionResult(response);
  SimulateFilterPassed();

  base::HistogramTester histogram_tester;
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // The contextual cue anchored message should be shown on the active tab.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Activate the background tab and have it navigate to a new URL.
  browser()->GetTabStripModel()->ActivateTabAt(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.othertab.com/2")));

  // Activate the original foreground tab.
  browser()->GetTabStripModel()->ActivateTabAt(1);

  // The contextual cue anchored message should not be shown on the active
  // tab.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !observer.GetCurrentPageActionState().anchored_message_showing;
  }));
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

  // URL won't match whatever navigated since it does not match the active
  // tab.
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
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  cue_target()->page_eligible = false;
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
      browser()->GetTabStripModel()->GetActiveWebContents();
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
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicDefaultTabContextEnabled, true);

  // Navigate current Chrome tab to a valid URL (and will be in the background
  // in final state).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.someurl.com/abc")));

  // Create a new tab that is specifically a URL that would normally be
  // skipped (will be in the background in final state).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://settings"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Navigate to a new eligible tab to be in the foreground (current active
  // tab).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  auto response = MakeCompleteResponse();
  auto* cue = response.mutable_contextual_cues(0);
  cue->set_suggested_cuj("TestCUJ");

  content::WebContents* active_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
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
  EXPECT_EQ(model_observer.content_->expand_button_accessible_name,
            u"Tab sharing details. Sharing 1 tab from www.example.com");
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  cue_target()->eligible = false;
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

  ASSERT_FALSE(cue_target()->HasClickData());

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Expect Shown event
  EXPECT_CALL(
      *GetMockPrivateInsightsService(),
      LogContextualCueEvent(testing::AllOf(
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::event_type,
              private_insights::events::ContextualCueLogEvent::SHOWN),
          testing::Property(&private_insights::events::ContextualCueLogEvent::
                                shown_duration_ms,
                            testing::Eq(0)))))
      .Times(1);

  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);

  // Expect Clicked event
  EXPECT_CALL(
      *GetMockPrivateInsightsService(),
      LogContextualCueEvent(testing::AllOf(
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::event_type,
              private_insights::events::ContextualCueLogEvent::CLICKED),
          testing::Property(&private_insights::events::ContextualCueLogEvent::
                                shown_duration_ms,
                            testing::Gt(0)),
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::cue_context,
              testing::ResultOf(
                  [](const auto& ctx) { return ctx.active_page().url(); },
                  testing::Eq("https://www.activetab.com/abc"))))))
      .Times(1);

  action->InvokeAction();

  ASSERT_TRUE(cue_target()->HasClickData());
  EXPECT_EQ("Prompt",
            std::get<GlicCueActionData>(cue_target()->click_data).prompt);
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
  EXPECT_GT(*duration_value, 0);

  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_CueInteraction::
          kProactiveCueInteractionName,
      static_cast<int64_t>(ContextualCueingInteraction::kCueClicked));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       ShowCueAndClick_ContextualSearchSurface) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Make kGlic ineligible for the page and register a kContextualSearch
  // target that is eligible for the page.
  cue_target()->page_eligible = false;
  class TestContextualSearchCueTarget : public TestCueTarget {
   public:
    CueActionData CueActionDataFromResponse(
        const optimization_guide::proto::ContextualCue& cue,
        std::vector<tabs::TabHandle> tabs_to_show) const override {
      ContextualSearchCueActionData data;
      if (cue.has_contextual_search_surface()) {
        data.query = cue.contextual_search_surface().query();
      }
      data.tabs_to_share = std::move(tabs_to_show);
      return data;
    }
    optimization_guide::proto::ContextualCueingSurface GetSurface()
        const override {
      return optimization_guide::proto::
          CONTEXTUAL_CUEING_SURFACE_CONTEXTUAL_SEARCH;
    }
  };
  auto contextual_search_target =
      std::make_unique<TestContextualSearchCueTarget>();
  TestCueTarget* contextual_search_target_ptr = contextual_search_target.get();
  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kContextualSearch, std::move(contextual_search_target));

  optimization_guide::proto::ContextualCueingResponse response;
  auto* cue = response.add_contextual_cues();
  cue->mutable_anchored_message_cue()->set_action_text("Search action");
  cue->mutable_anchored_message_cue()->set_anchored_message_text(
      "Search message");
  cue->mutable_contextual_search_surface()->set_query("Search query");

  base::HistogramTester histogram_tester;
  SeedExecutionResult(response);
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  ASSERT_TRUE(contextual_search_target_ptr->HasClickData());
  EXPECT_EQ("Search query", std::get<ContextualSearchCueActionData>(
                                contextual_search_target_ptr->click_data)
                                .query);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       ShowCueAndClick_DoesNotHideIfIsPersistent) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_FALSE(cue_target()->HasClickData());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  cue_target()->is_persistent = true;

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Expect Shown event.
  EXPECT_CALL(
      *GetMockPrivateInsightsService(),
      LogContextualCueEvent(testing::AllOf(
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::event_type,
              private_insights::events::ContextualCueLogEvent::SHOWN),
          testing::Property(&private_insights::events::ContextualCueLogEvent::
                                shown_duration_ms,
                            testing::Eq(0)))))
      .Times(1);

  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);

  // Initially, the contextual cue anchored message is shown on the screen.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Expect Clicked event.
  EXPECT_CALL(
      *GetMockPrivateInsightsService(),
      LogContextualCueEvent(testing::AllOf(
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::event_type,
              private_insights::events::ContextualCueLogEvent::CLICKED),
          testing::Property(&private_insights::events::ContextualCueLogEvent::
                                shown_duration_ms,
                            testing::Gt(0)),
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::cue_context,
              testing::ResultOf(
                  [](const auto& ctx) { return ctx.active_page().url(); },
                  testing::Eq("https://www.activetab.com/abc"))))))
      .Times(1);

  action->InvokeAction();

  ASSERT_TRUE(cue_target()->HasClickData());
  EXPECT_EQ("Prompt",
            std::get<GlicCueActionData>(cue_target()->click_data).prompt);

  // The cue should still be showing.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();

  EXPECT_TRUE(observer.GetCurrentPageActionState().showing);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueInteraction",
                                      ContextualCueingInteraction::kCueClicked,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       ShowCueAndClickAsIcon) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_FALSE(cue_target()->HasClickData());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  // Target click handler was not invoked.
  EXPECT_FALSE(cue_target()->HasClickData());

  // Anchored message is showing again.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       AccessibleNameUpdatesWithAnchoredMessageState) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);

  // 1. Setup observer to track accessible name.
  class AccessibleNameObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      accessible_name_ = model.GetAccessibleName();
    }
    std::u16string accessible_name_;
  };

  AccessibleNameObserver name_observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      name_observation(&name_observer);
  page_action_controller->AddObserver(kActionAnchoredContextualCue,
                                      name_observation);

  // Also keep standard observer to wait for states.
  page_actions::PageActionObserver state_observer(kActionAnchoredContextualCue);
  state_observer.RegisterAsPageActionObserver(*page_action_controller);

  base::HistogramTester histogram_tester;

  // 2. Trigger the cue.
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 3. Verify it is shown and has the short cue as the accessible name.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return state_observer.GetCurrentPageActionState().anchored_message_showing;
  }));
  EXPECT_EQ(name_observer.accessible_name_, u"Action text");

  // 4. Hide the anchored message.
  page_action_controller->HideAnchoredMessage(kActionAnchoredContextualCue);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !state_observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // 5. Verify name persists with it hiding into smaller action / icon.
  EXPECT_EQ(name_observer.accessible_name_, u"Action text");
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       TooltipUpdatesWithAnchoredMessageState) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);

  // 1. Setup observer to track tooltip.
  class TooltipObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      tooltip_ = model.GetTooltipText();
    }
    std::u16string tooltip_;
  };

  TooltipObserver tooltip_observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      tooltip_observation(&tooltip_observer);
  page_action_controller->AddObserver(kActionAnchoredContextualCue,
                                      tooltip_observation);

  // Also keep standard observer to wait for states.
  page_actions::PageActionObserver state_observer(kActionAnchoredContextualCue);
  state_observer.RegisterAsPageActionObserver(*page_action_controller);

  base::HistogramTester histogram_tester;

  // 2. Trigger the cue.
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 3. Verify it is shown and has the short cue as the tooltip.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return state_observer.GetCurrentPageActionState().anchored_message_showing;
  }));
  EXPECT_EQ(state_observer.GetCurrentPageActionState().tooltip, u"Action text");
  EXPECT_EQ(tooltip_observer.tooltip_, u"Action text");

  // 4. Hide the anchored message.
  page_action_controller->HideAnchoredMessage(kActionAnchoredContextualCue);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !state_observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // 5. Verify tooltip persists with it hiding into smaller action / icon.
  EXPECT_TRUE(state_observer.GetCurrentPageActionState().showing);
  EXPECT_FALSE(state_observer.GetCurrentPageActionState().chip_showing);
  EXPECT_EQ(state_observer.GetCurrentPageActionState().tooltip, u"Action text");
  EXPECT_EQ(tooltip_observer.tooltip_, u"Action text");

  // 6. Click the page action icon to re-show the anchored message.
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return state_observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // 7. Click again while anchored message is shown to invoke the cue and hide
  // it.
  action->InvokeAction();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !state_observer.GetCurrentPageActionState().showing; }));
  EXPECT_EQ(tooltip_observer.tooltip_, u"");
}

class ContextualCueingControllerMockOptGuideBrowserTest
    : public ContextualCueingControllerBrowserTest {
 public:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    ContextualCueingControllerBrowserTest::OnWillCreateBrowserContextServices(
        context);
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
  }

  MockOptimizationGuideKeyedService* mock_opt_guide() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->GetProfile()));
  }

  void AddLocalVisit(history::HistoryService* history_service,
                     const GURL& url,
                     const std::u16string& title,
                     base::Time visit_time,
                     bool is_known_to_sync = true) {
    class AddLocalVisitTask : public history::HistoryDBTask {
     public:
      AddLocalVisitTask(const GURL& url,
                        const std::u16string& title,
                        base::Time visit_time,
                        bool is_known_to_sync)
          : url_(url),
            title_(title),
            visit_time_(visit_time),
            is_known_to_sync_(is_known_to_sync) {}

      bool RunOnDBThread(history::HistoryBackend* backend,
                         history::HistoryDatabase* db) override {
        history::URLRow url_row(url_);
        url_row.set_title(title_);
        url_row.set_visit_count(1);
        url_row.set_last_visit(visit_time_);
        backend->AddPagesWithDetails({url_row}, history::SOURCE_BROWSED);
        if (is_known_to_sync_) {
          history::VisitRow visit_row;
          if (backend->GetLastVisitByTime(visit_time_, &visit_row)) {
            static_cast<history::HistoryBackendForSync*>(backend)
                ->MarkVisitAsKnownToSync(visit_row.visit_id);
          }
        }
        return true;
      }

      void DoneRunOnMainThread() override {}

     private:
      GURL url_;
      std::u16string title_;
      base::Time visit_time_;
      bool is_known_to_sync_;
    };
    history_service->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<AddLocalVisitTask>(url, title, visit_time,
                                            is_known_to_sync),
        &history_task_tracker_);
  }

 protected:
  base::CancelableTaskTracker history_task_tracker_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMockOptGuideBrowserTest,
                       InactiveTabShowsQuietCueAfterModelExecution) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#else
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  tabs::TabInterface* tab_1 = browser()->GetActiveTabInterface();
  page_actions::PageActionController* tab_1_page_action_controller =
      page_actions::PageActionController::From(tab_1);
  CHECK(tab_1_page_action_controller);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      visible_ = model.GetVisible();
      anchored_message_showing_ = model.ShouldShowAnchoredMessage();
    }
    bool visible_ = false;
    bool anchored_message_showing_ = false;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  tab_1_page_action_controller->AddObserver(kActionAnchoredContextualCue,
                                            observation);

  optimization_guide::OptimizationGuideModelExecutionResultCallback
      saved_callback;
  EXPECT_CALL(
      *mock_opt_guide(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContextualCueing,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) { saved_callback = std::move(callback); });

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SimulateFilterPassed();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !saved_callback.is_null(); }));

  // Open new tab in foreground right away.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Run the saved callback with the response while tab 1 is inactive.
  auto response = MakeCompleteResponse();
  optimization_guide::proto::Any response_any;
  response.SerializeToString(response_any.mutable_value());
  response_any.set_type_url(
      base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
  std::move(saved_callback)
      .Run(optimization_guide::OptimizationGuideModelExecutionResult(
               response_any, /*execution_info=*/nullptr),
           nullptr);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);

  // Switch back to Tab 1.
  browser()->GetTabStripModel()->ActivateTabAt(
      browser()->GetTabStripModel()->GetIndexOfTab(tab_1));

  // Once Tab 1 is active, the suggestion chip should be visible quietly
  // without showing the anchored message bubble.
  EXPECT_TRUE(observer.visible_);
  EXPECT_FALSE(observer.anchored_message_showing_);

  // Click the suggestion chip to expand the anchored message.
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  EXPECT_TRUE(observer.visible_);
  EXPECT_TRUE(observer.anchored_message_showing_);
#endif
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMockOptGuideBrowserTest,
                       TabNavigatedAwayAfterModelExecution) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  optimization_guide::OptimizationGuideModelExecutionResultCallback
      saved_callback;
  EXPECT_CALL(
      *mock_opt_guide(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContextualCueing,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) { saved_callback = std::move(callback); });

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SimulateFilterPassed();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !saved_callback.is_null(); }));

  // Navigate the tab away to a different URL before model execution response
  // arrives.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.example.com/def")));

  // Run the saved callback with the response.
  auto response = MakeCompleteResponse();
  optimization_guide::proto::Any response_any;
  response.SerializeToString(response_any.mutable_value());
  response_any.set_type_url(
      base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
  std::move(saved_callback)
      .Run(optimization_guide::OptimizationGuideModelExecutionResult(
               response_any, /*execution_info=*/nullptr),
           nullptr);

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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueNotShowingBecauseAnotherAnchoredMessageOpen) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#else
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  ASSERT_TRUE(page_action_controller);

  // Show an anchored message using another action ID.
  page_action_controller->ShowAnchoredMessage(
      kActionSidePanelShowReadAnything,
      {.priority = page_actions::PageActionPriorityCategory::kQuietUtility});

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kAnchoredMessageAlreadyShowing, 1);
  VerifyProactiveCueDecision(
      ukm_recorder, ContextualCueingDecision::kAnchoredMessageAlreadyShowing);
#endif
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueNotShowingBecauseTabInSplitView) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP() << "Contextual cueing split view not supported on Android";
#else
  // Add a second tab so we can check split view.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  // Put the active tab in split view.
  browser()->GetTabStripModel()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  ASSERT_TRUE(browser()->GetTabStripModel()->GetActiveTab()->IsSplit());

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kTabInSplitView,
                                      1);
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kTabInSplitView);
#endif
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest, HistorySyncOff) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
        ui::mojom::WindowOpenDisposition::kNewForegroundTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

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
        browser(), GURL("https://www.activetab.com/def")));

    // Wait until previous page action is gone.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return !observer.GetCurrentPageActionState().anchored_message_showing;
    }));

    SimulateFilterPassed(GURL("https://www.activetab.com/def"));

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
                       HomePageNotEligible_NoTrailingSlash) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Simulate a new page load.
  GURL homepage_url("https://activetab.com/us/en");
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  // Open side panel.
  auto* side_panel_ui = SidePanelUI::From(browser());
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
  auto* side_panel_ui = SidePanelUI::From(browser());
  ASSERT_TRUE(side_panel_ui);
  side_panel_ui->Show(SidePanelEntryId::kBookmarks);

  // Verify that our observer successfully intercepted the open event
  // and hid the contextual cue dynamically.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().showing; }));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueDoesNotHideWhenSidePanelOpenedIfIsPersistent) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  cue_target()->is_persistent = true;

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
  auto* side_panel_ui = SidePanelUI::From(browser());
  ASSERT_TRUE(side_panel_ui);
  side_panel_ui->Show(SidePanelEntryId::kBookmarks);

  // Verify that the cue remains showing.
  // Wait a short duration to ensure it doesn't get hidden.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();

  EXPECT_TRUE(observer.GetCurrentPageActionState().showing);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CueNotShowingBecauseInfobarVisible) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  // Add an infobar to the active tab.
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager->AddInfoBar(ConfirmInfoBar::Create(
      ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
      std::make_unique<TestInfoBarDelegate>()));
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
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

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HidesOnCrossOriginNavigation) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  // Navigate to an eligible URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  // Show the cue.
  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Perform a cross-origin/different site navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.example.com/other")));

  // Verify that the cue is hidden.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().showing; }));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HidesOnBackForwardNavigation) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  // Navigate to page 1.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/1"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Navigate to page 2.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  page_actions::PageActionController* page_action_controller =
      GetPageActionController();
  CHECK(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  // Show cue on page 2.
  base::HistogramTester histogram_tester;
  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Perform a back/forward (history) navigation using the back button.
  chrome::GoBack(browser(), ui::mojom::WindowOpenDisposition::kCurrentTab);
  content::WaitForLoadStop(
      browser()->GetTabStripModel()->GetActiveWebContents());

  // Verify that the cue is hidden.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().showing; }));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       RecordsShownCueCUJHistogram) {
  // 1. Navigate to a valid eligible URL
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  // 7. Verify that the interaction was logged with the hashed CUJ!
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueInteraction",
                                      ContextualCueingInteraction::kCueClicked,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Clicked",
      base::HashMetricName("test_cuj_string"), 1);
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.CueShown.PageType.Pdf",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.V2.CueInteraction.PageType.Pdf", 0);

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

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       RecordsCueInteractionOnPdfPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  // 1. Navigate to a PDF document URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), pdf_url, ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;

  // 2. Mock the server response and inject a fake CUJ string.
  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->set_suggested_cuj("pdf_cuj_string");
  SeedExecutionResult(std::move(response));

  // 3. Trigger the cue execution flow.
  SimulateFilterPassed(pdf_url);

  // 4. Wait for the flow to successfully finish.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // 5. Confirm flow was completed successfully. There is a sample recorded to
  // both the general and PDF-specific "CueShown" metric.
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueShown",
                                      base::HashMetricName("pdf_cuj_string"),
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueShown.PageType.Pdf",
      base::HashMetricName("pdf_cuj_string"), 1);

  // 6. Simulate user clicking the cue.
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  // 7. Verify that the interaction was logged to both the general and
  // PDF-specific "CueInteraction" metric.
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.CueInteraction",
                                      ContextualCueingInteraction::kCueClicked,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.PageType.Pdf",
      ContextualCueingInteraction::kCueClicked, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Clicked",
      base::HashMetricName("pdf_cuj_string"), 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       RecordsFallbackToTargetNameWhenCUJEmpty) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;

  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->clear_suggested_cuj();
  SeedExecutionResult(std::move(response));

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ShownCueCUJ",
      base::HashMetricName(GetName(CueTargetType::kGlic)), 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueShown",
      base::HashMetricName(GetName(CueTargetType::kGlic)), 1);
}

class ContextualCueingControllerBrowserTestWithAgeRestriction
    : public ContextualCueingControllerBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {kContextualCueingV2, kContextualCueingV2EnforceAgeRestriction},
        /*disabled_features=*/{kContextualCueingV2MultiSource});
  }

  void SetUserRestriction(bool is_restricted) {
    auto account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(!is_restricted);
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTestWithAgeRestriction,
                       AgeRestrictionEnforced) {
  SetUserRestriction(/*is_restricted=*/true);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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

class ContextualCueingControllerShowInSplitViewBrowserTest
    : public ContextualCueingControllerBrowserTestBase {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualCueingV2,
          {{"ContextualCueingV2DiscardShoppingPdfs", "true"},
           {"ContextualCueingV2TabListVisibility", "always"},
           {"ContextualCueingV2ShouldShowCueInSplitView", "true"}}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction,
                               kContextualCueingV2MultiSource});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerShowInSplitViewBrowserTest,
                       ShowCueInSplitView) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP() << "Contextual cueing split view not supported on Android";
#else
  // Add a second tab so we can check split view.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SeedExecutionResult(MakeCompleteResponse());

  // Put the active tab in split view.
  browser()->GetTabStripModel()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  ASSERT_TRUE(browser()->GetTabStripModel()->GetActiveTab()->IsSplit());

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
#endif
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       RecordsCueInteractionDismissed) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EXPECT_CALL(
      *GetMockPrivateInsightsService(),
      LogContextualCueEvent(testing::AllOf(
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::event_type,
              private_insights::events::ContextualCueLogEvent::SHOWN),
          testing::Property(&private_insights::events::ContextualCueLogEvent::
                                shown_duration_ms,
                            testing::Eq(0)))))
      .Times(1);

  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->set_suggested_cuj("test_cuj_string");
  SeedExecutionResult(response);

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  EXPECT_CALL(
      *GetMockPrivateInsightsService(),
      LogContextualCueEvent(testing::AllOf(
          testing::Property(
              &private_insights::events::ContextualCueLogEvent::event_type,
              private_insights::events::ContextualCueLogEvent::DISMISSED),
          testing::Property(&private_insights::events::ContextualCueLogEvent::
                                shown_duration_ms,
                            testing::Ge(0)))))
      .Times(1);

  // Directly call OnCueInteraction to simulate the Menu Delegate
  auto* cue = &response.contextual_cues(0);
  ASSERT_FALSE(cue_target()->dismissed);
  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kGlic, *cue,
      {}, {}, "test_cuj_string", {}, "fake_id");
  EXPECT_TRUE(cue_target()->dismissed);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.Dismissed",
      base::HashMetricName("test_cuj_string"), 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueInteraction::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0].get(),
      ukm::builders::ContextualCueing_CueInteraction::
          kProactiveCueInteractionName,
      static_cast<int64_t>(ContextualCueingInteraction::kCueDismissed));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       NotifiesTargetOfSuggestionsSettingsOpened) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  // Opening the settings page activates another tab, so keep this tab's target.
  TestCueTarget* target = cue_target();
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->suggestions_settings_opened);

  auto response = MakeCompleteResponse();
  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueSuggestionsSettings,
      CueTargetType::kGlic, response.contextual_cues(0), {}, {},
      "test_cuj_string", {}, "fake_id");

  EXPECT_TRUE(target->suggestions_settings_opened);
  EXPECT_FALSE(target->dismissed);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       RecordsCueInteractionEditPrompt) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto response = MakeCompleteResponse();
  response.mutable_contextual_cues(0)->set_suggested_cuj("test_cuj_string");
  SeedExecutionResult(response);

  SimulateFilterPassed();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  auto* cue = &response.contextual_cues(0);
  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueEditPrompt, CueTargetType::kGlic, *cue,
      {}, {}, "test_cuj_string", {}, "fake_id");

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.CueInteraction.EditPrompt",
      base::HashMetricName("test_cuj_string"), 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueInteraction::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0].get(),
      ukm::builders::ContextualCueing_CueInteraction::
          kProactiveCueInteractionName,
      static_cast<int64_t>(ContextualCueingInteraction::kCueEditPrompt));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       TabMovedToNewWindowMaintainsContextualCue) {
#if BUILDFLAG(IS_ANDROID)
  GTEST_SKIP()
      << "Contextual cueing anchored message not implemented for Android";
#endif

  ASSERT_FALSE(cue_target()->HasClickData());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  page_actions::PageActionController* first_controller =
      GetPageActionController();
  CHECK(first_controller);
  page_actions::PageActionObserver first_observer(kActionAnchoredContextualCue);
  first_observer.RegisterAsPageActionObserver(*first_controller);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SeedExecutionResult(MakeCompleteResponse());
  SimulateFilterPassed();
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return first_observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Create second browser and move the active tab to it.
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->GetTabStripModel()->DetachTabAtForInsertion(/*index=*/1);
  second_browser->GetTabStripModel()->InsertDetachedTabAt(
      /*index=*/0, std::move(detached_tab), AddTabTypes::ADD_ACTIVE);

  page_actions::PageActionController* second_controller =
      page_actions::PageActionController::From(
          second_browser->GetActiveTabInterface());
  CHECK(second_controller);
  page_actions::PageActionObserver second_observer(
      kActionAnchoredContextualCue);
  second_observer.RegisterAsPageActionObserver(*second_controller);

  // The page action should still be showing on the tab in the second window.
  EXPECT_TRUE(second_observer.GetCurrentPageActionState().showing);
  EXPECT_FALSE(
      second_observer.GetCurrentPageActionState().anchored_message_showing);

  // Invoke the action on the second browser window. The first click opens the
  // anchored message.
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(second_browser)->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return second_observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Second click invokes the action.
  action->InvokeAction();

  TestCueTarget* second_cue_target = static_cast<TestCueTarget*>(
      ContextualCueingController::From(second_browser->GetActiveTabInterface())
          ->GetTarget(CueTargetType::kGlic));
  ASSERT_TRUE(second_cue_target);
  ASSERT_TRUE(second_cue_target->HasClickData());
  EXPECT_EQ("Prompt",
            std::get<GlicCueActionData>(second_cue_target->click_data).prompt);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !second_observer.GetCurrentPageActionState().showing; }));
}

class ContextualCueingControllerMultiSourceBrowserTest
    : public ContextualCueingControllerBrowserTestBase {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualCueingV2,
          {{"ContextualCueingV2DiscardShoppingPdfs", "true"},
           {"ContextualCueingV2TabListVisibility", "always"},
           {"ContextualCueingV2EnablePrivateInsightsLogging", "true"}}},
         {kContextualCueingV2MultiSource, {}},
         {kContextualCueingV2AllowOverridingUcbScoring, {}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       HistorySyncOff_LocalGeneratorSucceeds) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  cue_target()->eligible = false;
  auto test_source_target = std::make_unique<TestCueTarget>();
  test_source_target->eligible = true;
  test_source_target->generate_result =
      MakeCompleteResponse().contextual_cues(0);
  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kTestSource, std::move(test_source_target));

  EnableHistorySync(false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       HistorySyncOff_ModelExecutionTargetBlocked) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  cue_target()->eligible = true;
  cue_target()->generate_result = std::nullopt;
  SeedExecutionResult(MakeCompleteResponse());

  EnableHistorySync(false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kHistorySyncOff,
                                      1);
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kHistorySyncOff);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       Navigation_EvaluatesCuesWithEarlySignals) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  cue_target()->eligible = false;
  auto test_source_target = std::make_unique<TestCueTarget>();
  test_source_target->eligible = true;
  test_source_target->generate_result =
      MakeCompleteResponse().contextual_cues(0);
  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kTestSource, std::move(test_source_target));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       TabActivation_DoesNotTriggerEvaluation) {
  base::HistogramTester histogram_tester;

  // Start embedded test server to serve real pages.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Seed the execution result so we get kSuccess instead of failure.
  SeedExecutionResult(MakeCompleteResponse());

  // 1. Load First Tab
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Wait for initial load evaluation to complete.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.Decision", 1);

  // 2. Open and Load Second Tab (Foreground)
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Wait for second load evaluation to complete.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 2);
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.Decision", 2);

  // 3. Switch back to First Tab
  browser()->tab_strip_model()->ActivateTabAt(0);

  // 4. Verify Evaluation DID NOT happen again
  // yield control to ensure no async tasks were accidentally queued.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Total count should still be 2.
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.Decision", 2);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       SameDocumentNavigation_TriggersEvaluation) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Start embedded test server to serve real pages.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Disable initial eligibility so the first page load doesn't trigger a cue.
  cue_target()->eligible = false;
  auto test_source_target = std::make_unique<TestCueTarget>();
  test_source_target->eligible = false;
  test_source_target->generate_result =
      MakeCompleteResponse().contextual_cues(0);
  auto* test_source_target_ptr = test_source_target.get();
  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kTestSource, std::move(test_source_target));

  // 1. Load initial page with ineligible target.
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kTargetFeatureNotEligible, 1);

  // 2. Make the target eligible and perform same-document navigation.
  test_source_target_ptr->eligible = true;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "history.pushState({}, '', '/title2.html');"));
  observer.Wait();

  // 3. Verify cue evaluation triggers on same-document navigation.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 2);
  histogram_tester.ExpectBucketCount("ContextualCueing.V2.Decision",
                                     ContextualCueingDecision::kSuccess, 1);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueShown::kEntryName);
  ASSERT_EQ(2u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[1].get(),
      ukm::builders::ContextualCueing_CueShown::kProactiveCueDecisionName,
      static_cast<int64_t>(ContextualCueingDecision::kSuccess));
}

class TargetRegisteringObserver : public TabStripModelObserver {
 public:
  explicit TargetRegisteringObserver(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {
    tab_strip_model_->AddObserver(this);
  }
  ~TargetRegisteringObserver() override {
    tab_strip_model_->RemoveObserver(this);
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents : change.GetInsert()->contents) {
        auto non_mes_target = std::make_unique<TestCueTarget>();
        non_mes_target->requires_model_execution = false;
        non_mes_target->supported_intrusiveness = {CueIntrusiveness::kLoud,
                                                   CueIntrusiveness::kQuiet};
        non_mes_target->generate_result =
            MakeCompleteResponse().contextual_cues(0);
        ContextualCueingController::From(contents.tab)
            ->RegisterCueTarget(CueTargetType::kTestSource,
                                std::move(non_mes_target));
      }
    }
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       InactiveTab_EvaluatesQuietCuesButNotLoudCues) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  SeedExecutionResult(MakeCompleteResponse());

  // 1. Open a background tab with only Glic target (which only supports loud
  // cues). Because the tab is inactive, intrusiveness is downgraded from loud
  // to quiet. Since Glic target does not support quiet cues, no candidate
  // targets are eligible.
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kTargetFeatureNotEligible, 1);

  // 2. Set up observer to register quiet-capable target on newly inserted
  // tabs.
  TargetRegisteringObserver target_observer(browser()->tab_strip_model());

  // 3. Open a background tab with quiet-capable target.
  // Even though service allowed tier is loud, it is downgraded to quiet
  // because tab is inactive. The quiet-capable target evaluates successfully.
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // The background tab navigated to url2 and triggered quiet cue evaluation
  // on load.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 2);
  histogram_tester.ExpectBucketCount("ContextualCueing.V2.Decision",
                                     ContextualCueingDecision::kSuccess, 1);
}

class AsyncTestCueTarget : public TestCueTarget {
 public:
  void CheckEligibility(base::WeakPtr<content::WebContents> web_contents,
                        CueIntrusiveness intrusiveness,
                        EligibilityCallback callback) override {
    saved_callback_ = std::move(callback);
  }

  void ReplyEligibility(bool eligible, ContentGenerator generator) {
    if (saved_callback_) {
      std::move(saved_callback_).Run(eligible, std::move(generator));
    }
  }

 private:
  EligibilityCallback saved_callback_;
};

IN_PROC_BROWSER_TEST_F(
    ContextualCueingControllerMultiSourceBrowserTest,
    TabDeactivatedDuringEligibilityChecks_FiltersOutLoudOnlyTargetsAndEvaluatesQuiet) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  SeedExecutionResult(MakeCompleteResponse());

  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  auto loud_target = std::make_unique<AsyncTestCueTarget>();
  loud_target->supported_intrusiveness = {CueIntrusiveness::kLoud};
  auto* loud_target_ptr = loud_target.get();
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kGlic,
                                                    std::move(loud_target));

  auto quiet_target = std::make_unique<AsyncTestCueTarget>();
  quiet_target->supported_intrusiveness = {CueIntrusiveness::kLoud,
                                           CueIntrusiveness::kQuiet};
  auto* quiet_target_ptr = quiet_target.get();
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kTestSource,
                                                    std::move(quiet_target));

  // Trigger evaluation on active tab (intrusiveness is loud).
  contextual_cueing_controller()->EvaluateCues();

  // Deactivate tab 0 by opening a new foreground tab.
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Complete async eligibility checks for tab 0 while it is inactive.
  loud_target_ptr->ReplyEligibility(true, base::NullCallback());
  quiet_target_ptr->ReplyEligibility(
      true,
      base::BindOnce(
          [](optimization_guide::proto::ContextualCue cue,
             base::OnceCallback<void(
                 std::optional<optimization_guide::proto::ContextualCue>)> cb) {
            std::move(cb).Run(std::move(cue));
          },
          MakeCompleteResponse().contextual_cues(0)));

  // Quiet target should be selected and succeed on tab 0 despite tab 0
  // becoming inactive.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 3);
  histogram_tester.ExpectBucketCount("ContextualCueing.V2.Decision",
                                     ContextualCueingDecision::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       MultiSourceOverridesUcbScoring) {
  base::HistogramTester histogram_tester;

  auto low_priority_target = std::make_unique<TestCueTarget>();
  low_priority_target->eligible = true;
  low_priority_target->generate_result =
      MakeCompleteResponse().contextual_cues(0);

  auto override_target = std::make_unique<TestCueTarget>();
  override_target->eligible = true;
  override_target->overrides_ucb_scoring = true;
  override_target->generate_result = MakeCompleteResponse().contextual_cues(0);
  auto* override_target_ptr = override_target.get();

  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kGlic, std::move(low_priority_target));
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kIndigo,
                                                    std::move(override_target));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  EXPECT_TRUE(override_target_ptr->chip_shown ||
              override_target_ptr->anchored_message_shown_priority.has_value());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       EvaluateCueForTarget_OnlyEvaluatesThatTarget) {
  base::HistogramTester histogram_tester;

  // Nothing is eligible when the page loads.
  cue_target()->eligible = false;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);

  // A target that would win any evaluation it's part of.
  auto override_target = std::make_unique<TestCueTarget>();
  override_target->overrides_ucb_scoring = true;
  override_target->generate_result = MakeCompleteResponse().contextual_cues(0);
  auto* override_target_ptr = override_target.get();
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kIndigo,
                                                    std::move(override_target));
  auto test_source_target = std::make_unique<TestCueTarget>();
  test_source_target->generate_result =
      MakeCompleteResponse().contextual_cues(0);
  auto* test_source_target_ptr = test_source_target.get();
  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kTestSource, std::move(test_source_target));

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  contextual_cueing_controller()->EvaluateCueForTarget(
      CueTargetType::kTestSource);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 2);
  histogram_tester.ExpectBucketCount("ContextualCueing.V2.Decision",
                                     ContextualCueingDecision::kSuccess, 1);
  EXPECT_TRUE(
      test_source_target_ptr->chip_shown ||
      test_source_target_ptr->anchored_message_shown_priority.has_value());
  EXPECT_FALSE(
      override_target_ptr->chip_shown ||
      override_target_ptr->anchored_message_shown_priority.has_value());
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.CueShown", 1);
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.CueShownLatency", 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_CueShown::kEntryName);
  ASSERT_EQ(1u, entries.size());
  EXPECT_TRUE(ukm_recorder.GetEntryMetric(
      entries[0].get(), ukm::builders::ContextualCueing_CueShown::
                            kProactiveCueLatencyAfterPageLoadName));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       EvaluateCueForTarget_DoesNotReplaceExistingCueChip) {
  base::HistogramTester histogram_tester;

  cue_target()->eligible = false;
  auto first_target = std::make_unique<TestCueTarget>();
  first_target->generate_result = MakeCompleteResponse().contextual_cues(0);
  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kContextualSearch, std::move(first_target));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // Downgrade to chip/icon only so GetActiveAnchoredMessage() is nullopt while
  // the contextual cue page action remains visible for this page load.
  contextual_cueing_controller()->HideAnchoredMessage();
  ASSERT_FALSE(GetPageActionController()->GetActiveAnchoredMessage());
  page_actions::PageActionObserver cue_observer(kActionAnchoredContextualCue);
  cue_observer.RegisterAsPageActionObserver(*GetPageActionController());
  ASSERT_TRUE(cue_observer.GetCurrentPageActionState().showing);

  auto second_target = std::make_unique<TestCueTarget>();
  second_target->supported_intrusiveness = {CueIntrusiveness::kLoud,
                                            CueIntrusiveness::kQuiet};
  second_target->generate_result = MakeCompleteResponse().contextual_cues(0);
  auto* second_target_ptr = second_target.get();
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kTestSource,
                                                    std::move(second_target));

  contextual_cueing_controller()->EvaluateCueForTarget(
      CueTargetType::kTestSource);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 2);
  histogram_tester.ExpectBucketCount(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kAnchoredMessageAlreadyShowing, 1);
  EXPECT_FALSE(second_target_ptr->chip_shown ||
               second_target_ptr->anchored_message_shown_priority.has_value());
  EXPECT_TRUE(cue_observer.GetCurrentPageActionState().showing);
}

class ContextualCueingControllerMultiSourceWithAgeRestrictionBrowserTest
    : public ContextualCueingControllerBrowserTestBase {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {kContextualCueingV2, kContextualCueingV2MultiSource,
         kContextualCueingV2EnforceAgeRestriction},
        /*disabled_features=*/{});
  }

  void SetUserRestriction(bool is_restricted) {
    auto account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(!is_restricted);
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualCueingControllerMultiSourceWithAgeRestrictionBrowserTest,
    AgeRestriction_LocalGeneratorSucceeds) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  cue_target()->eligible = false;
  auto test_source_target = std::make_unique<TestCueTarget>();
  test_source_target->eligible = true;
  test_source_target->generate_result =
      MakeCompleteResponse().contextual_cues(0);
  contextual_cueing_controller()->RegisterCueTarget(
      CueTargetType::kTestSource, std::move(test_source_target));

  SetUserRestriction(/*is_restricted=*/true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);
  VerifyProactiveCueDecision(ukm_recorder, ContextualCueingDecision::kSuccess);
}

IN_PROC_BROWSER_TEST_F(
    ContextualCueingControllerMultiSourceWithAgeRestrictionBrowserTest,
    AgeRestriction_ModelExecutionTargetBlocked) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  cue_target()->eligible = true;
  cue_target()->generate_result = std::nullopt;
  SeedExecutionResult(MakeCompleteResponse());

  SetUserRestriction(/*is_restricted=*/true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.activetab.com/abc")));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kAgeRestrictionEnforced, 1);
  VerifyProactiveCueDecision(ukm_recorder,
                             ContextualCueingDecision::kAgeRestrictionEnforced);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       QuietCueFallbackWhenLoudCapsExceeded) {
  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(service);

  // Register a non-MES target that supports quiet cues.
  auto non_mes_target = std::make_unique<TestCueTarget>();
  non_mes_target->requires_model_execution = false;
  non_mes_target->supported_intrusiveness = {CueIntrusiveness::kLoud,
                                             CueIntrusiveness::kQuiet};
  non_mes_target->generate_result = MakeCompleteResponse().contextual_cues(0);
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kTestSource,
                                                    std::move(non_mes_target));

  // Exhaust loud caps by showing a loud cue.
  service->OnCueShown(GURL("https://example.com"), CueTargetType::kGlic,
                      /*record_ucb_stats=*/true, CueIntrusiveness::kLoud);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      visible_ = model.GetVisible();
      anchored_message_showing_ = model.ShouldShowAnchoredMessage();
    }
    bool visible_ = false;
    bool anchored_message_showing_ = false;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  GetPageActionController()->AddObserver(kActionAnchoredContextualCue,
                                         observation);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kCurrentTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // The chip should be visible, but anchored message should NOT be showing
  // (quiet mode).
  EXPECT_TRUE(observer.visible_);
  EXPECT_FALSE(observer.anchored_message_showing_);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       MESOnlyTargetBlockedWhenCapsExceeded) {
  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(service);

  // Override the default target on the tab to explicitly require MES.
  cue_target()->requires_model_execution = true;

  // Exhaust loud caps by showing a loud cue.
  service->OnCueShown(GURL("https://example.com"), CueTargetType::kGlic,
                      /*record_ucb_stats=*/true, CueIntrusiveness::kLoud);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      visible_ = model.GetVisible();
    }
    bool visible_ = false;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  GetPageActionController()->AddObserver(kActionAnchoredContextualCue,
                                         observation);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kCurrentTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.Decision",
      ContextualCueingDecision::kTargetFeatureNotEligible, 1);

  EXPECT_FALSE(observer.visible_);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       DowngradesToQuietOnDismiss) {
  cue_target()->eligible = false;
  auto target = std::make_unique<TestCueTarget>();
  target->requires_model_execution = false;
  target->supported_intrusiveness = {CueIntrusiveness::kLoud,
                                     CueIntrusiveness::kQuiet};
  target->downgrades_to_quiet_on_dismiss = true;
  target->generate_result = MakeCompleteResponse().contextual_cues(0);
  TestCueTarget* target_ptr = target.get();
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kTestSource,
                                                    std::move(target));

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      visible_ = model.GetVisible();
      anchored_message_showing_ = model.ShouldShowAnchoredMessage();
    }
    bool visible_ = false;
    bool anchored_message_showing_ = false;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  GetPageActionController()->AddObserver(kActionAnchoredContextualCue,
                                         observation);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kCurrentTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  EXPECT_TRUE(observer.visible_);
  EXPECT_TRUE(observer.anchored_message_showing_);

  // Now dismiss the cue.
  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kTestSource,
      *target_ptr->generate_result, {}, {}, "cuj", {}, "fake_id");

  // The chip should still be visible, but anchored message is NOT showing.
  EXPECT_TRUE(observer.visible_);
  EXPECT_FALSE(observer.anchored_message_showing_);

  // When clicking the suggestion chip, it should expand out into an anchored
  // message.
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  EXPECT_TRUE(observer.anchored_message_showing_);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMultiSourceBrowserTest,
                       QuietCueAllowedAfterDismissal) {
  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(service);

  // Register a non-MES target that supports quiet cues.
  auto non_mes_target = std::make_unique<TestCueTarget>();
  non_mes_target->requires_model_execution = false;
  non_mes_target->supported_intrusiveness = {CueIntrusiveness::kLoud,
                                             CueIntrusiveness::kQuiet};
  non_mes_target->generate_result = MakeCompleteResponse().contextual_cues(0);
  TestCueTarget* target_ptr = non_mes_target.get();
  contextual_cueing_controller()->RegisterCueTarget(CueTargetType::kTestSource,
                                                    std::move(non_mes_target));

  // User dismisses a cue.
  service->OnCueDismissed(CueTargetType::kGlic, /*record_ucb_stats=*/true);

  class TestObserver : public page_actions::PageActionModelObserver {
   public:
    void OnPageActionModelChanged(
        const page_actions::PageActionModelInterface& model) override {
      visible_ = model.GetVisible();
      anchored_message_showing_ = model.ShouldShowAnchoredMessage();
    }
    bool visible_ = false;
    bool anchored_message_showing_ = false;
  };

  TestObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  GetPageActionController()->AddObserver(kActionAnchoredContextualCue,
                                         observation);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kCurrentTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester, "ContextualCueing.V2.Decision", 1);
  histogram_tester.ExpectUniqueSample("ContextualCueing.V2.Decision",
                                      ContextualCueingDecision::kSuccess, 1);

  // The chip should still be visible, but anchored message is NOT showing.
  EXPECT_TRUE(observer.visible_);
  EXPECT_FALSE(observer.anchored_message_showing_);
  EXPECT_TRUE(target_ptr->chip_shown);

  // When clicking the suggestion chip, it should expand out into an anchored
  // message.
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();

  EXPECT_TRUE(observer.visible_);
  EXPECT_TRUE(observer.anchored_message_showing_);
  EXPECT_TRUE(target_ptr->chip_clicked);
  EXPECT_EQ(target_ptr->anchored_message_shown_priority,
            page_actions::PageActionPriorityCategory::kUserInteraction);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HatsSurveyLaunchedOnDismiss_EducationAndShopping) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->GetProfile(), base::BindRepeating(&BuildMockHatsService)));
  ASSERT_TRUE(mock_hats_service);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  EXPECT_FALSE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kEducation));
  EXPECT_FALSE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kShopping));

  auto response = MakeCompleteResponse();
  auto* cue = &response.contextual_cues(0);

  base::OnceClosure edu_success_callback;
  EXPECT_CALL(*mock_hats_service,
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerContextualCueingDismissed, testing::_,
                  testing::IsEmpty(),
                  testing::UnorderedElementsAre(
                      testing::Pair("CUJ", "LEARNING"),
                      testing::Pair("hostname", "www.activetab.com")),
                  testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&](const std::string& trigger,
                    content::WebContents* web_contents,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const std::optional<std::string>& supplied_trigger_id,
                    const HatsService::SurveyOptions& survey_options) {
        edu_success_callback = std::move(success_callback);
        return HatsService::LaunchError::kNone;
      });

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"LEARNING",
      /*action=*/{}, /*cue_id=*/"fake_id");

  // Before success callback runs, pref should still be false (Option 2A).
  EXPECT_FALSE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kEducation));
  ASSERT_TRUE(edu_success_callback);
  std::move(edu_success_callback).Run();
  EXPECT_TRUE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kEducation));

  // Dismissing another EDU cue should NOT launch the survey again.
  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(kHatsSurveyTriggerContextualCueingDismissed,
                                 testing::_, testing::_, testing::_, testing::_,
                                 testing::_, testing::_, testing::_))
      .Times(0);

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"QUIZ_ME",
      /*action=*/{}, /*cue_id=*/"fake_id_2");

  // Dismissing a Shopping cue SHOULD launch the survey because Shopping is
  // tracked independently from EDU.
  base::OnceClosure shopping_success_callback;
  EXPECT_CALL(*mock_hats_service,
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerContextualCueingDismissed, testing::_,
                  testing::IsEmpty(),
                  testing::UnorderedElementsAre(
                      testing::Pair("CUJ", "SHOPPING"),
                      testing::Pair("hostname", "www.activetab.com")),
                  testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&](const std::string& trigger,
                    content::WebContents* web_contents,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const std::optional<std::string>& supplied_trigger_id,
                    const HatsService::SurveyOptions& survey_options) {
        shopping_success_callback = std::move(success_callback);
        return HatsService::LaunchError::kNone;
      });

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"SHOPPING",
      /*action=*/{}, /*cue_id=*/"fake_id_3");

  ASSERT_TRUE(shopping_success_callback);
  std::move(shopping_success_callback).Run();
  EXPECT_TRUE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kShopping));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HatsSurveyLaunchedOnDismiss_Indigo) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->GetProfile(), base::BindRepeating(&BuildMockHatsService)));
  ASSERT_TRUE(mock_hats_service);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  EXPECT_FALSE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kIndigo));

  auto response = MakeCompleteResponse();
  auto* cue = &response.contextual_cues(0);

  base::OnceClosure indigo_success_callback;
  EXPECT_CALL(*mock_hats_service,
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerContextualCueingDismissed, testing::_,
                  testing::IsEmpty(),
                  testing::UnorderedElementsAre(
                      testing::Pair("CUJ", "TRY_ON_YOU"),
                      testing::Pair("hostname", "www.activetab.com")),
                  testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&](const std::string& trigger,
                    content::WebContents* web_contents,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const std::optional<std::string>& supplied_trigger_id,
                    const HatsService::SurveyOptions& survey_options) {
        indigo_success_callback = std::move(success_callback);
        return HatsService::LaunchError::kNone;
      });

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kIndigo, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"Indigo",
      /*action=*/{}, /*cue_id=*/"fake_id");

  ASSERT_TRUE(indigo_success_callback);
  std::move(indigo_success_callback).Run();
  EXPECT_TRUE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kIndigo));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HatsSurveyDismiss_RetryOnFailure) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->GetProfile(), base::BindRepeating(&BuildMockHatsService)));
  ASSERT_TRUE(mock_hats_service);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  auto response = MakeCompleteResponse();
  auto* cue = &response.contextual_cues(0);

  base::OnceClosure failure_cb;
  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(kHatsSurveyTriggerContextualCueingDismissed,
                                 testing::_, testing::_, testing::_, testing::_,
                                 testing::_, testing::_, testing::_))
      .WillOnce([&](const std::string& trigger,
                    content::WebContents* web_contents,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const std::optional<std::string>& supplied_trigger_id,
                    const HatsService::SurveyOptions& survey_options) {
        failure_cb = std::move(failure_callback);
        return HatsService::LaunchError::kNone;
      });

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"LEARNING",
      /*action=*/{}, /*cue_id=*/"fake_id");

  ASSERT_TRUE(failure_cb);
  std::move(failure_cb).Run();
  EXPECT_FALSE(prefs::HasDismissSurveyBeenShown(
      prefs, ContextualCueSurveyCategory::kEducation));

  // Subsequent dismissal should retry launching the survey since previous
  // attempt failed.
  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(kHatsSurveyTriggerContextualCueingDismissed,
                                 testing::_, testing::_, testing::_, testing::_,
                                 testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(HatsService::LaunchError::kNone));

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"LEARNING",
      /*action=*/{}, /*cue_id=*/"fake_id_2");
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       HatsSurveyNotLaunchedOnNonDismissInteractions) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->GetProfile(), base::BindRepeating(&BuildMockHatsService)));
  ASSERT_TRUE(mock_hats_service);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_CALL(*mock_hats_service,
              LaunchSurveyForWebContents(testing::_, testing::_, testing::_,
                                         testing::_, testing::_, testing::_,
                                         testing::_, testing::_))
      .Times(0);

  auto response = MakeCompleteResponse();
  auto* cue = &response.contextual_cues(0);

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueClicked, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"LEARNING",
      /*action=*/{}, /*cue_id=*/"fake_id_1");

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueEditPrompt, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"LEARNING",
      /*action=*/{}, /*cue_id=*/"fake_id_2");

  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueSuggestionsSettings,
      CueTargetType::kGlic, *cue, /*tabs_to_show=*/{}, /*background_tabs=*/{},
      /*cuj=*/"LEARNING", /*action=*/{}, /*cue_id=*/"fake_id_3");

  // Dismissing a cue with an unknown survey category should not launch a
  // survey.
  contextual_cueing_controller()->OnCueInteraction(
      ContextualCueingInteraction::kCueDismissed, CueTargetType::kGlic, *cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"TRAVEL",
      /*action=*/{}, /*cue_id=*/"fake_id_4");
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerBrowserTest,
                       CategoryClassifierObservation_TogglesWithHistorySync) {
  FakePageCategoryClassifierBridge* bridge =
      AttachFakeCategoryClassifierBridge();
  EXPECT_TRUE(bridge->demand_active());
  EXPECT_TRUE(
      GetTestSyncService()->HasObserver(contextual_cueing_controller()));
  glic::GlicCueTabState* tab_state =
      glic::GlicCueTabState::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(tab_state);
  EXPECT_FALSE(GetTestSyncService()->HasObserver(tab_state));
  contextual_search::ContextualSearchCueTabState* cs_tab_state =
      contextual_search::ContextualSearchCueTabState::From(
          browser()->GetActiveTabInterface());
  ASSERT_TRUE(cs_tab_state);
  EXPECT_FALSE(GetTestSyncService()->HasObserver(cs_tab_state));

  // Open a second tab and verify closing it keeps demand active while the
  // first tab remains open with History Sync enabled.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/tab2"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(bridge->demand_active());
  browser()->GetTabStripModel()->CloseWebContentsAt(1,
                                                    TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(bridge->demand_active());

  EnableHistorySync(false);
  EXPECT_FALSE(bridge->demand_active());

  EnableHistorySync(true);
  EXPECT_TRUE(bridge->demand_active());

  contextual_cueing_controller()->OnSyncShutdown(GetTestSyncService());
  EXPECT_FALSE(bridge->demand_active());
  EXPECT_FALSE(
      GetTestSyncService()->HasObserver(contextual_cueing_controller()));

  EnableHistorySync(true);
  EXPECT_FALSE(bridge->demand_active());
}

IN_PROC_BROWSER_TEST_F(
    ContextualCueingControllerMultiSourceBrowserTest,
    CategoryClassifierObservation_DisabledInControllerAndGatedInTabState) {
  FakePageCategoryClassifierBridge* bridge =
      AttachFakeCategoryClassifierBridge();
  glic::GlicCueTabState* tab_state =
      glic::GlicCueTabState::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(tab_state);
  contextual_search::ContextualSearchCueTabState* cs_tab_state =
      contextual_search::ContextualSearchCueTabState::From(
          browser()->GetActiveTabInterface());
  ASSERT_TRUE(cs_tab_state);

  // With V2MultiSource and History Sync enabled, GlicCueTabState and
  // ContextualSearchCueTabState observe SyncService and kCategoryClassifier,
  // while ContextualCueingController observes neither.
  EXPECT_TRUE(bridge->demand_active());
  EXPECT_FALSE(
      GetTestSyncService()->HasObserver(contextual_cueing_controller()));
  EXPECT_TRUE(GetTestSyncService()->HasObserver(tab_state));
  EXPECT_TRUE(GetTestSyncService()->HasObserver(cs_tab_state));

  // Open a second tab and verify closing it keeps demand active while the
  // first tab remains open with History Sync enabled.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/tab2"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(
      GetTestSyncService()->HasObserver(contextual_cueing_controller()));
  EXPECT_TRUE(bridge->demand_active());
  browser()->GetTabStripModel()->CloseWebContentsAt(1,
                                                    TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(bridge->demand_active());

  // When History Sync is disabled, neither ContextualCueingController nor
  // the tab states observe kCategoryClassifier.
  EnableHistorySync(false);
  EXPECT_FALSE(bridge->demand_active());

  // Detach GlicCueTabState and ContextualSearchCueTabState to isolate
  // ContextualCueingController and verify that ContextualCueingController
  // does not register for kCategoryClassifier when
  // kContextualCueingV2MultiSource is enabled, even with History Sync on.
  tab_state->SetAnnotationServiceForTesting(nullptr);
  cs_tab_state->SetAnnotationServiceForTesting(nullptr);

  EnableHistorySync(true);
  EXPECT_FALSE(bridge->demand_active());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerMockOptGuideBrowserTest,
                       HistoricalTabsDisabledByDefault) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  AddLocalVisit(history_service, GURL("https://www.history1.com/article"),
                u"History 1", base::Time::Now() - base::Minutes(5));

  std::optional<optimization_guide::proto::ContextualCueingRequest>
      captured_request;
  EXPECT_CALL(
      *mock_opt_guide(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContextualCueing,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            captured_request = static_cast<
                const optimization_guide::proto::ContextualCueingRequest&>(
                request_metadata);
          });

  base::HistogramTester histogram_tester;
  SimulateFilterPassed();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return captured_request.has_value(); }));

  EXPECT_EQ(0, captured_request->historical_tabs_size());
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.NumRequestedHistoricalTabs", 0, 1);
}

class ContextualCueingControllerHistoricalTabsBrowserTest
    : public ContextualCueingControllerMockOptGuideBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualCueingV2,
          {{"ContextualCueingV2DiscardShoppingPdfs", "true"},
           {"ContextualCueingV2TabListVisibility", "always"},
           {"ContextualCueingV2EnablePrivateInsightsLogging", "true"},
           {"ContextualCueingV2MaxNumHistoricalTabs", "5"}}}},
        /*disabled_features=*/{kContextualCueingV2EnforceAgeRestriction,
                               kContextualCueingV2MultiSource});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerHistoricalTabsBrowserTest,
                       HistoricalTabsPopulatedAndDeduplicated) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.backgroundtab.com/page"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);

  const base::Time now = base::Time::Now();
  // Active and background tab URLs (including with #ref) should be
  // deduplicated.
  AddLocalVisit(history_service, GURL("https://www.activetab.com/abc#section"),
                u"Active Tab With Ref", now - base::Minutes(1));
  AddLocalVisit(history_service, GURL("https://www.backgroundtab.com/page#top"),
                u"Background Tab With Ref", now - base::Minutes(2));
  // Non-HTTP(S) URLs should be excluded.
  AddLocalVisit(history_service, GURL("file:///tmp/local.html"), u"Local File",
                now - base::Minutes(3));
  // Visits older than kHistoricalTabsTimeWindow (24h) should be excluded.
  AddLocalVisit(history_service, GURL("https://www.oldvisit.com/page"),
                u"Old Visit", now - base::Hours(25));
  // Visits from a foreign device (non-empty `originator_cache_guid`) should
  // be excluded.
  class AddForeignVisitTask : public history::HistoryDBTask {
   public:
    explicit AddForeignVisitTask(base::Time visit_time)
        : visit_time_(visit_time) {}
    bool RunOnDBThread(history::HistoryBackend* backend,
                       history::HistoryDatabase* db) override {
      history::VisitRow foreign_visit;
      foreign_visit.visit_time = visit_time_;
      foreign_visit.transition = ui::PageTransitionFromInt(
          ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
          ui::PAGE_TRANSITION_CHAIN_END);
      foreign_visit.originator_cache_guid = "foreign_device_guid";
      foreign_visit.is_known_to_sync = true;
      backend->AddSyncedVisit(GURL("https://www.foreignvisit.com/page"),
                              u"Foreign Visit", /*hidden=*/false, foreign_visit,
                              std::nullopt, std::nullopt);
      return true;
    }
    void DoneRunOnMainThread() override {}

   private:
    base::Time visit_time_;
  };
  history_service->ScheduleDBTask(
      FROM_HERE, std::make_unique<AddForeignVisitTask>(now - base::Minutes(4)),
      &history_task_tracker_);
  // Local visits not known to sync should be excluded.
  AddLocalVisit(history_service, GURL("https://www.unsyncedvisit.com/page"),
                u"Unsynced Visit", now - base::Minutes(5),
                /*is_known_to_sync=*/false);

  // Eligible visits (including search URLs and #ref deduplication within
  // history).
  AddLocalVisit(history_service, GURL("https://www.history1.com/article"),
                u"History 1", now - base::Minutes(30));
  AddLocalVisit(history_service,
                GURL("https://www.google.com/search?q=test+query"),
                u"Google Search", now - base::Minutes(20));
  AddLocalVisit(history_service, GURL("https://www.history2.com/doc#part1"),
                u"History 2 Old", now - base::Minutes(15));
  AddLocalVisit(history_service, GURL("https://www.history2.com/doc#part2"),
                u"History 2 New", now - base::Minutes(10));

  std::optional<optimization_guide::proto::ContextualCueingRequest>
      captured_request;
  EXPECT_CALL(
      *mock_opt_guide(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContextualCueing,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            captured_request = static_cast<
                const optimization_guide::proto::ContextualCueingRequest&>(
                request_metadata);
          });

  base::HistogramTester histogram_tester;
  SimulateFilterPassed();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return captured_request.has_value(); }));

  ASSERT_EQ(3, captured_request->historical_tabs_size());
  EXPECT_EQ("https://www.history2.com/doc#part2",
            captured_request->historical_tabs(0).url());
  EXPECT_EQ("History 2 New", captured_request->historical_tabs(0).title());
  EXPECT_FALSE(captured_request->historical_tabs(0).has_tab_id());

  EXPECT_EQ("https://www.google.com/search?q=test+query",
            captured_request->historical_tabs(1).url());
  EXPECT_EQ("Google Search", captured_request->historical_tabs(1).title());
  EXPECT_FALSE(captured_request->historical_tabs(1).has_tab_id());

  EXPECT_EQ("https://www.history1.com/article",
            captured_request->historical_tabs(2).url());
  EXPECT_EQ("History 1", captured_request->historical_tabs(2).title());
  EXPECT_FALSE(captured_request->historical_tabs(2).has_tab_id());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.NumRequestedHistoricalTabs", 3, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingControllerHistoricalTabsBrowserTest,
                       MaxHistoricalTabsRespected) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.activetab.com/abc"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);

  const base::Time now = base::Time::Now();
  const int total_visits = kMaxNumHistoricalTabs.Get() + 5;
  for (int i = 0; i < total_visits; ++i) {
    AddLocalVisit(history_service,
                  GURL(base::StringPrintf("https://www.history.com/%d", i)),
                  base::UTF8ToUTF16(base::StringPrintf("History %d", i)),
                  now - base::Minutes(i + 1));
  }

  std::optional<optimization_guide::proto::ContextualCueingRequest>
      captured_request;
  EXPECT_CALL(
      *mock_opt_guide(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kContextualCueing,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            captured_request = static_cast<
                const optimization_guide::proto::ContextualCueingRequest&>(
                request_metadata);
          });

  base::HistogramTester histogram_tester;
  SimulateFilterPassed();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return captured_request.has_value(); }));

  EXPECT_EQ(kMaxNumHistoricalTabs.Get(),
            captured_request->historical_tabs_size());
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.V2.NumRequestedHistoricalTabs",
      kMaxNumHistoricalTabs.Get(), 1);
}

}  // namespace
}  // namespace contextual_cueing
