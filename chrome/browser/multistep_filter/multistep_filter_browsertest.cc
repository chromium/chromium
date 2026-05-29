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
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller_test_api.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_test_utils.h"
#include "components/multistep_filter/core/annotation_index/fake_annotation_index_server.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_service_test_api.h"
#include "components/multistep_filter/core/switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

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
constexpr char kAllowedDomainsParam[] = "allowed_domains";

}  // namespace

class MultistepFilterBrowserTest
    : public InProcessBrowserTest,
      public MultistepFilterService::ObserverForTest {
 public:
  MultistepFilterBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kMultistepFilter,
        {{kAllowedDomainsParam,
          std::string(kTestAllowedDomain) + "," + kTestAllowedDomain2},
         {kCueTemplatesMap.name,
          "{\"test_task\": {\"template\": \"Template\"}}"}});
  }
  ~MultistepFilterBrowserTest() override = default;

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&MultistepFilterBrowserTest::HandleHtmlRequest,
                            base::Unretained(this)));
    fake_server_.Initialize(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kMultistepFilterIndexServerApiBaseUrl,
        embedded_test_server()->GetURL("/").spec());
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
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                        signin::ConsentLevel::kSignin);

    service_ =
        MultistepFilterServiceFactory::GetForProfile(browser()->profile());
    if (service_) {
      test_api(*service_).SetObserverForTest(this);
    }
  }

  void TearDownOnMainThread() override {
    if (service_) {
      test_api(*service_).SetObserverForTest(nullptr);
    }
    service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

#if !BUILDFLAG(IS_CHROMEOS)
  void ClearPrimaryAccount() {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::ClearPrimaryAccount(identity_manager);
  }
#endif

  FakeAnnotationIndexServer& fake_server() { return fake_server_; }

  // MultistepFilterService::ObserverForTest:
  void OnExtractionFinished(std::optional<base::Uuid> annotation_id) override {
    extraction_future_.SetValue(annotation_id);
  }

  void OnSuggestionGenerated(
      std::optional<UrlFilterSuggestion> suggestion) override {
    suggestion_future_.SetValue(suggestion);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleHtmlRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == kExtractionUrlPath ||
        request.relative_url == kSuggestionTriggerUrlPath ||
        request.relative_url == kSuggestionUrlPath) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content("<html><body>hello</body></html>");
      response->set_content_type("text/html");
      return response;
    }
    return nullptr;
  }

  FakeAnnotationIndexServer fake_server_;
  raw_ptr<MultistepFilterService> service_ = nullptr;

 protected:
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

  fake_server().SetExtractResponse(CreateExtractTaskAttributesResponse(
      kTestAllowedDomain, kTestTaskType,
      {{kTestAttributeKey, kTestAttributeValue},
       {kTestAttributeKey2, kTestAttributeValue2}}));
  fake_server().SetSupportedTasksResponse(
      CreateSupportedTasksResponse({kTestTaskType}));

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
  fake_server().SetExecutionStrategiesResponse(execution_strategies_response);
  fake_server().SetExtractResponse(ExtractTaskAttributesResponse());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), suggestion_trigger_url));
  EXPECT_FALSE(extraction_future_.Take().has_value());
  EXPECT_TRUE(suggestion_future_.Take().has_value());

  const std::optional<GetTaskExecutionStrategiesRequest>& strategies_request =
      fake_server().GetLastStrategiesRequest();
  ASSERT_TRUE(strategies_request.has_value());
  ASSERT_EQ(strategies_request->candidates_size(), 1);
  EXPECT_EQ(strategies_request->candidates(0).candidate_id(),
            annotation_id.AsLowercaseString());

  FilterUiController* ui_controller =
      FilterUiController::From(browser()->tab_strip_model()->GetActiveTab());
  ASSERT_TRUE(ui_controller);
  const std::optional<UrlFilterSuggestion>& suggestion_result =
      test_api(*ui_controller).current_url_filter_suggestion();
  ASSERT_TRUE(suggestion_result.has_value());
  EXPECT_EQ(suggestion_result->navigation_url, suggestion_url);

  ToastController* toast_controller =
      browser()->browser_window_features()->toast_controller();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return toast_controller->IsShowingToast() &&
           toast_controller->GetCurrentToastId() ==
               ToastId::kMultistepFilterSuggestionRecent;
  }));

  toasts::ToastView* toast_view = toast_controller->GetToastViewForTesting();
  ASSERT_TRUE(toast_view);
  views::MdTextButton* action_button = toast_view->action_button_for_testing();
  ASSERT_TRUE(action_button);

  content::TestNavigationObserver nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  ui::MouseEvent click_event(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(action_button).NotifyClick(click_event);

  nav_observer.Wait();

  EXPECT_EQ(suggestion_url, browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetLastCommittedURL());
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
}

#if !BUILDFLAG(IS_CHROMEOS)
// Tests that no extraction or suggestion occurs if the user logs out of Chrome.
IN_PROC_BROWSER_TEST_F(MultistepFilterBrowserTest,
                       NoExtractionOrSuggestionWhenNotSignedIn) {
  GURL extraction_url =
      embedded_test_server()->GetURL(kTestAllowedDomain, kExtractionUrlPath);
  GURL suggestion_trigger_url = embedded_test_server()->GetURL(
      kTestAllowedDomain2, kSuggestionTriggerUrlPath);
  GURL suggestion_url =
      embedded_test_server()->GetURL(kTestAllowedDomain2, kSuggestionUrlPath);

  fake_server().SetExtractResponse(CreateExtractTaskAttributesResponse(
      kTestAllowedDomain, kTestTaskType,
      {{kTestAttributeKey, kTestAttributeValue},
       {kTestAttributeKey2, kTestAttributeValue2}}));
  fake_server().SetSupportedTasksResponse(
      CreateSupportedTasksResponse({kTestTaskType}));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extraction_url));
  EXPECT_TRUE(extraction_future_.Take().has_value());
  EXPECT_FALSE(suggestion_future_.Take().has_value());

  fake_server().SetExecutionStrategiesResponse(
      CreateTaskExecutionStrategiesResponse(
          suggestion_url, {{kTestAttributeKey, kTestAttributeValue},
                           {kTestAttributeKey2, kTestAttributeValue2}}));
  ClearPrimaryAccount();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), suggestion_trigger_url));
  EXPECT_FALSE(extraction_future_.Take().has_value());
  EXPECT_FALSE(suggestion_future_.Take().has_value());

  FilterUiController* ui_controller =
      FilterUiController::From(browser()->tab_strip_model()->GetActiveTab());
  ASSERT_TRUE(ui_controller);
  EXPECT_FALSE(
      test_api(*ui_controller).current_url_filter_suggestion().has_value());

  ToastController* toast_controller =
      browser()->browser_window_features()->toast_controller();
  EXPECT_FALSE(toast_controller->IsShowingToast());
}
#endif

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
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);

  MultistepFilterService* service =
      MultistepFilterServiceFactory::GetForProfile(browser()->profile());

  EXPECT_EQ(service, nullptr);
}

}  // namespace multistep_filter
