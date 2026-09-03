// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"
#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer_test_api.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller_test_api.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/multistep_filter/content/content_filter_navigation_observer_test_api.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_test_utils.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/filter_tab_controller_test_api.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/switches.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::OptimizationGuideDecision;
using ::optimization_guide::OptimizationGuideDecisionWithMetadata;
using ::optimization_guide::OptimizationMetadata;
using ::optimization_guide::proto::OptimizationType;
using ::optimization_guide::proto::RequestContext;

constexpr char kTestEmail[] = "test@example.com";
constexpr char kTestAllowedDomain[] = "example.com";
constexpr char kTestAllowedDomain2[] = "example2.com";
constexpr char kExtractionUrlPath[] = "/extraction.html";
constexpr char kSuggestionTriggerUrlPath[] = "/suggestion_trigger.html";
constexpr char kSuggestionUrlPath[] = "/suggestion.html";
constexpr char kTestTaskType[] = "test_task";
constexpr char kTestAttributeKey[] = "color";
constexpr char kTestAttributeValue[] = "red";
constexpr char kTestAttributeKey2[] = "size";
constexpr char kTestAttributeValue2[] = "large";

FilterTabController* GetTabController(BrowserWindowInterface* browser) {
  tabs::TabInterface* active_tab = browser->GetTabStripModel()->GetActiveTab();
  if (!active_tab) {
    return nullptr;
  }
  ChromeFilterNavigationObserver* chrome_observer =
      ChromeFilterNavigationObserver::From(active_tab);
  if (!chrome_observer) {
    return nullptr;
  }
  ContentFilterNavigationObserver* content_observer =
      test_api(*chrome_observer).GetObserver();
  if (!content_observer) {
    return nullptr;
  }
  return test_api(*content_observer).GetTabController();
}

}  // namespace

class MultistepFilterBrowserTest : public InProcessBrowserTest,
                                   public FilterTabController::ObserverForTest {
 public:
  MultistepFilterBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kMultistepFilter);
  }
  ~MultistepFilterBrowserTest() override = default;

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&MultistepFilterBrowserTest::HandleHtmlRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kMultistepFilterAllowHttpForTesting);
  }

  void SetUpInProcessBrowserTestFixture() override {
    network::TestNetworkConnectionTracker::GetInstance();
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();

    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->GetProfile());
    // TODO(crbug.com/519167729): Remove once kSync becomes unreachable or is
    // deleted from the codebase.
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, kTestEmail, signin::ConsentLevel::kSync);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    browser()->GetProfile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    auto* sync_service =
        SyncServiceFactory::GetForProfile(browser()->GetProfile());
    std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker =
        sync_service->GetSetupInProgressHandle();
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});

    service_ =
        MultistepFilterServiceFactory::GetForProfile(browser()->GetProfile());
    FilterTabController* controller = GetTabController(browser());
    if (controller) {
      test_api(*controller).SetObserverForTest(this);
    }
    optimization_guide_decider_ =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->GetProfile());
  }

  void TearDownOnMainThread() override {
    FilterTabController* controller = GetTabController(browser());
    if (controller) {
      test_api(*controller).SetObserverForTest(nullptr);
    }
    service_ = nullptr;
    optimization_guide_decider_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

#if !BUILDFLAG(IS_CHROMEOS)
  void ClearPrimaryAccount() {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->GetProfile());
    signin::ClearPrimaryAccount(identity_manager);
  }
#endif

  // FilterTabController::ObserverForTest:
  void OnExtractionFinishedForTest(
      std::optional<base::Uuid> annotation_id) override {
    extraction_future_.SetValue(annotation_id);
  }

  void OnSuggestionGeneratedForTest(
      std::optional<UrlFilterSuggestion> suggestion) override {
    suggestion_future_.SetValue(suggestion);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleHtmlRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("<html><body>hello</body></html>");
    response->set_content_type("text/html");
    return response;
  }

 protected:
  raw_ptr<MultistepFilterService> service_ = nullptr;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_decider_ = nullptr;
  base::test::TestFuture<std::optional<base::Uuid>> extraction_future_;
  base::test::TestFuture<std::optional<UrlFilterSuggestion>> suggestion_future_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MultistepFilterBrowserTest,
                       ExtractsAnnotationAndGeneratesSuggestionOnNavigation) {
  GURL extraction_url =
      embedded_test_server()->GetURL(kTestAllowedDomain, kExtractionUrlPath);
  GURL suggestion_trigger_url = embedded_test_server()->GetURL(
      kTestAllowedDomain2, kSuggestionTriggerUrlPath);
  GURL suggestion_url =
      embedded_test_server()->GetURL(kTestAllowedDomain2, kSuggestionUrlPath);

  OptimizationMetadata supported_metadata = CreateOptimizationMetadata(
      AnyWrapProto(CreateSupportedTasksResponse({kTestTaskType})));
  OptimizationMetadata extract_metadata = CreateOptimizationMetadata(
      AnyWrapProto(CreateExtractTaskAttributesResponse(
          kTestTaskType, {{kTestAttributeKey, kTestAttributeValue},
                          {kTestAttributeKey2, kTestAttributeValue2}})));

  optimization_guide_decider_->AddHintWithMultipleOptimizationsForTesting(
      extraction_url,
      {{OptimizationType::FILTER_TASKS_SUPPORTED, supported_metadata},
       {OptimizationType::FILTER_EXTRACT_ATTRIBUTES, extract_metadata}});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extraction_url));

  std::optional<base::Uuid> extraction_result = extraction_future_.Take();
  ASSERT_TRUE(extraction_result.has_value());
  base::Uuid annotation_id = std::move(extraction_result).value();
  EXPECT_FALSE(suggestion_future_.Take().has_value());

  GetTaskExecutionStrategiesResponse execution_strategies_response =
      CreateTaskExecutionStrategiesResponse(
          suggestion_url, {{kTestAttributeKey, kTestAttributeValue},
                           {kTestAttributeKey2, kTestAttributeValue2}});
  execution_strategies_response.mutable_execution_strategies(0)
      ->set_candidate_id(annotation_id.AsLowercaseString());
  OptimizationMetadata execution_metadata = CreateOptimizationMetadata(
      AnyWrapProto(execution_strategies_response));
  OptimizationGuideDecisionWithMetadata execution_decision_with_metadata =
      CreateDecisionWithMetadata(OptimizationGuideDecision::kTrue,
                                 execution_metadata);

  optimization_guide_decider_->AddOnDemandHintForTesting(
      suggestion_trigger_url, OptimizationType::FILTER_EXECUTION_STRATEGY,
      execution_decision_with_metadata);
  optimization_guide_decider_->AddHintWithMultipleOptimizationsForTesting(
      suggestion_trigger_url,
      {{OptimizationType::FILTER_TASKS_SUPPORTED, supported_metadata},
       {OptimizationType::FILTER_EXTRACT_ATTRIBUTES, std::nullopt}});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), suggestion_trigger_url));
  EXPECT_FALSE(extraction_future_.Take().has_value());
  EXPECT_TRUE(suggestion_future_.Take().has_value());

  FilterUiController* ui_controller =
      FilterUiController::From(browser()->GetTabStripModel()->GetActiveTab());
  ASSERT_TRUE(ui_controller);
  const std::optional<FilterUiController::SuggestionState>& state =
      test_api(*ui_controller).suggestion_state();
  ASSERT_TRUE(state.has_value());
  const UrlFilterSuggestion& suggestion_result = state->suggestion;
  EXPECT_EQ(suggestion_result.navigation_url, suggestion_url);

  page_actions::PageActionController* page_action_controller =
      browser()
          ->GetTabStripModel()
          ->GetActiveTab()
          ->GetTabFeatures()
          ->page_action_controller();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return page_action_controller->GetActiveAnchoredMessage() ==
           kActionMultistepFilter;
  }));

  content::TestNavigationObserver nav_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  actions::ActionItem* action = actions::ActionManager::Get().FindAction(
      kActionMultistepFilter,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction(actions::ActionInvocationContext());

  nav_observer.Wait();

  EXPECT_EQ(suggestion_url, browser()
                                ->GetTabStripModel()
                                ->GetActiveWebContents()
                                ->GetLastCommittedURL());
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(MultistepFilterBrowserTest,
                       ClearHistoryDeletesSuggestions) {
  GURL extraction_url =
      embedded_test_server()->GetURL(kTestAllowedDomain, kExtractionUrlPath);

  OptimizationMetadata supported_metadata = CreateOptimizationMetadata(
      AnyWrapProto(CreateSupportedTasksResponse({kTestTaskType})));
  OptimizationMetadata extract_metadata = CreateOptimizationMetadata(
      AnyWrapProto(CreateExtractTaskAttributesResponse(
          kTestTaskType, {{kTestAttributeKey, kTestAttributeValue}})));

  optimization_guide_decider_->AddHintWithMultipleOptimizationsForTesting(
      extraction_url,
      {{OptimizationType::FILTER_TASKS_SUPPORTED, supported_metadata},
       {OptimizationType::FILTER_EXTRACT_ATTRIBUTES, extract_metadata}});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extraction_url));
  EXPECT_TRUE(extraction_future_.Take().has_value());

  base::test::TestFuture<std::vector<FilterAnnotation>> get_future1;
  service_->GetFilterStore()->GetAnnotationsForTasksSortedByCreationTimestamp(
      {kTestTaskType}, get_future1.GetCallback(), 10, base::Time());
  EXPECT_THAT(get_future1.Get(), testing::SizeIs(1));

  base::test::TestFuture<void> history_future;
  base::CancelableTaskTracker task_tracker;
  auto* history_service = HistoryServiceFactory::GetForProfile(
      browser()->GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  history_service->ExpireHistoryBetween(
      {}, std::nullopt, base::Time(), base::Time::Now(),
      /*user_initiated=*/true, history_future.GetCallback(), &task_tracker);
  ASSERT_TRUE(history_future.Wait());

  base::ThreadPoolInstance::Get()->FlushForTesting();

  base::test::TestFuture<std::vector<FilterAnnotation>> get_future2;
  service_->GetFilterStore()->GetAnnotationsForTasksSortedByCreationTimestamp(
      {kTestTaskType}, get_future2.GetCallback(), 10, base::Time());
  EXPECT_THAT(get_future2.Get(), testing::SizeIs(0));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MultistepFilterBrowserTest,
                       NoExtractionOrSuggestionWhenNotSignedIn) {
  GURL extraction_url =
      embedded_test_server()->GetURL(kTestAllowedDomain, kExtractionUrlPath);
  GURL suggestion_trigger_url = embedded_test_server()->GetURL(
      kTestAllowedDomain2, kSuggestionTriggerUrlPath);
  GURL suggestion_url =
      embedded_test_server()->GetURL(kTestAllowedDomain2, kSuggestionUrlPath);

  OptimizationMetadata supported_metadata = CreateOptimizationMetadata(
      AnyWrapProto(CreateSupportedTasksResponse({kTestTaskType})));
  OptimizationMetadata extract_metadata = CreateOptimizationMetadata(
      AnyWrapProto(CreateExtractTaskAttributesResponse(
          kTestTaskType, {{kTestAttributeKey, kTestAttributeValue},
                          {kTestAttributeKey2, kTestAttributeValue2}})));

  optimization_guide_decider_->AddHintWithMultipleOptimizationsForTesting(
      extraction_url,
      {{OptimizationType::FILTER_TASKS_SUPPORTED, supported_metadata},
       {OptimizationType::FILTER_EXTRACT_ATTRIBUTES, extract_metadata}});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extraction_url));
  EXPECT_TRUE(extraction_future_.Take().has_value());
  EXPECT_FALSE(suggestion_future_.Take().has_value());

  GetTaskExecutionStrategiesResponse execution_strategies_response =
      CreateTaskExecutionStrategiesResponse(
          suggestion_url, {{kTestAttributeKey, kTestAttributeValue},
                           {kTestAttributeKey2, kTestAttributeValue2}});
  OptimizationMetadata execution_metadata = CreateOptimizationMetadata(
      AnyWrapProto(execution_strategies_response));
  OptimizationGuideDecisionWithMetadata execution_decision_with_metadata =
      CreateDecisionWithMetadata(OptimizationGuideDecision::kTrue,
                                 execution_metadata);

  optimization_guide_decider_->AddOnDemandHintForTesting(
      suggestion_trigger_url, OptimizationType::FILTER_EXECUTION_STRATEGY,
      execution_decision_with_metadata);
  optimization_guide_decider_->AddHintWithMultipleOptimizationsForTesting(
      suggestion_trigger_url,
      {{OptimizationType::FILTER_TASKS_SUPPORTED, supported_metadata},
       {OptimizationType::FILTER_EXTRACT_ATTRIBUTES, std::nullopt}});

  ClearPrimaryAccount();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), suggestion_trigger_url));
  EXPECT_FALSE(extraction_future_.Take().has_value());
  EXPECT_FALSE(suggestion_future_.Take().has_value());

  FilterUiController* ui_controller =
      FilterUiController::From(browser()->GetTabStripModel()->GetActiveTab());
  ASSERT_TRUE(ui_controller);
  EXPECT_FALSE(test_api(*ui_controller).suggestion_state().has_value());

  ToastController* toast_controller = ToastController::From(browser());
  EXPECT_FALSE(toast_controller->IsShowingToast());
}
#endif

IN_PROC_BROWSER_TEST_F(MultistepFilterBrowserTest,
                       ExecuteSettingsCommandOpensAiPage) {
  tabs::TabInterface* active_tab =
      browser()->GetTabStripModel()->GetActiveTab();
  auto* ui_controller = multistep_filter::FilterUiController::From(active_tab);
  ASSERT_TRUE(ui_controller);

  multistep_filter::test_api(*ui_controller)
      .ExecuteCommand(multistep_filter::internal::kSettingsCommand, 0);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetTabStripModel()->GetActiveWebContents()->GetURL() ==
           GURL(base::StrCat({::chrome::kChromeUISettingsURL,
                              ::chrome::kSuggestionsSubPage}));
  }));
}

IN_PROC_BROWSER_TEST_F(MultistepFilterBrowserTest,
                       NoExtractionOrSuggestionWhenSmartSuggestionsDisabled) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing),
      static_cast<int>(
          optimization_guide::prefs::FeatureOptInState::kDisabled));

  GURL extraction_url =
      embedded_test_server()->GetURL(kTestAllowedDomain, kExtractionUrlPath);
  GURL suggestion_trigger_url = embedded_test_server()->GetURL(
      kTestAllowedDomain2, kSuggestionTriggerUrlPath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extraction_url));
  EXPECT_FALSE(extraction_future_.Take().has_value());
  EXPECT_FALSE(suggestion_future_.Take().has_value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), suggestion_trigger_url));
  EXPECT_FALSE(extraction_future_.Take().has_value());
  EXPECT_FALSE(suggestion_future_.Take().has_value());

  FilterUiController* ui_controller =
      FilterUiController::From(browser()->GetTabStripModel()->GetActiveTab());
  ASSERT_TRUE(ui_controller);
  EXPECT_FALSE(test_api(*ui_controller).suggestion_state().has_value());
}

class MultistepFilterDisabledBrowserTest : public InProcessBrowserTest {
 public:
  MultistepFilterDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(kMultistepFilter);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the `MultistepFilterService` is not created when the feature is
// disabled.
IN_PROC_BROWSER_TEST_F(MultistepFilterDisabledBrowserTest,
                       ServiceNotCreatedWhenFeatureDisabled) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);

  MultistepFilterService* service =
      MultistepFilterServiceFactory::GetForProfile(browser()->GetProfile());

  EXPECT_EQ(service, nullptr);
}

}  // namespace multistep_filter
