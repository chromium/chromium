// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_ai_mode_promo_tab_helper.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class BrowserView;

namespace contextual_tasks {

namespace {

class MockSearchAIModeSignInPromoController
    : public SearchAIModeSignInPromoController {
 public:
  explicit MockSearchAIModeSignInPromoController(
      content::WebContents* web_contents)
      : SearchAIModeSignInPromoController(web_contents) {}
  MOCK_METHOD(bool, MaybeShowPromo, (BrowserView*), (override));
};

}  // namespace

class SearchAiModePromoTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  SearchAiModePromoTabHelperBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{switches::kEnableSearchAIModeSigninPromo,
          {{"SearchAIModePromoPageLoadDelay", "0s"}}},
         {contextual_tasks::kContextualTasks, {}}},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&SearchAiModePromoTabHelperBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ai_url_ = GURL("https://www.google.com/ai");
    target_url_ = GURL("https://www.google.com/target");
    other_url_ = GURL("https://www.google.com/other");

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.host().find("google.com") !=
                  std::string::npos) {
                // Serve some content to all the urls in this test, so that the
                // navigations always commit.
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                    "<html><body></body></html>", params->client.get());
                return true;
              }
              return false;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&SearchAiModePromoTabHelperBrowserTest::
                                         BuildMockContextualTasksUiService,
                                     base::Unretained(this)));
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&SearchAiModePromoTabHelperBrowserTest::
                                         BuildMockAimEligibilityService,
                                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto aim_eligibility_service = std::make_unique<MockAimEligibilityService>(
        *profile->GetPrefs(), /*template_url_service=*/nullptr,
        /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

    ON_CALL(*aim_eligibility_service, IsAimEligible())
        .WillByDefault(testing::Return(true));
    ON_CALL(*aim_eligibility_service, IsCobrowseEligible())
        .WillByDefault(testing::Return(true));
    ON_CALL(*aim_eligibility_service, HasAimUrlParams(testing::_))
        .WillByDefault([this](const GURL& url) { return url == ai_url_; });
    return aim_eligibility_service;
  }

  std::unique_ptr<KeyedService> BuildMockContextualTasksUiService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto mock_ui_service =
        std::make_unique<testing::NiceMock<MockContextualTasksUiService>>(
            profile, ContextualTasksServiceFactory::GetForProfile(profile),
            IdentityManagerFactory::GetForProfile(profile),
            AimEligibilityServiceFactory::GetForProfile(profile));

    ON_CALL(*mock_ui_service, IsAiUrl(testing::_))
        .WillByDefault([this](const GURL& url) { return url == ai_url_; });
    return mock_ui_service;
  }

  std::unique_ptr<SearchAIModeSignInPromoController> CreateMockController(
      int expected_calls_count,
      content::WebContents* web_contents) {
    CHECK(web_contents);
    auto controller =
        std::make_unique<MockSearchAIModeSignInPromoController>(web_contents);
    EXPECT_CALL(*controller, MaybeShowPromo(testing::_))
        .Times(expected_calls_count)
        .WillRepeatedly(testing::Return(true));

    return controller;
  }

 protected:
  GURL ai_url_;
  GURL target_url_;
  GURL other_url_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::CallbackListSubscription create_services_subscription_;
};

// Tests that the helper is destroyed if the user navigates away after the
// promo is shown but before sign-in.
IN_PROC_BROWSER_TEST_F(SearchAiModePromoTabHelperBrowserTest,
                       AbortsFlowIfWebContentsNavigatesAway) {
  // 1. Navigate active tab to AI page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url_));
  content::WebContents* source_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 2. Open a new tab from the AI page.
  content::WebContentsAddedObserver observer;
  ASSERT_TRUE(content::ExecJs(source_contents,
                              "window.open('" + target_url_.spec() + "')",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  content::WebContents* new_contents = observer.GetWebContents();
  ASSERT_TRUE(new_contents);

  // 3. Verify helper has been created.
  auto* helper = SearchAiModePromoTabHelper::FromWebContents(new_contents);
  ASSERT_TRUE(helper);

  // Inject the promo controller factory.
  helper->SetSigninPromoControllerFactoryForTesting(base::BindRepeating(
      &SearchAiModePromoTabHelperBrowserTest::CreateMockController,
      base::Unretained(this), /*expected_calls_count=*/1));

  // Ensure navigation is finished and promo logic runs.
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // 4. Navigate away.
  ASSERT_TRUE(content::NavigateToURL(new_contents, other_url_));

  // 5. Verify helper is destroyed.
  EXPECT_FALSE(SearchAiModePromoTabHelper::FromWebContents(new_contents));
}

IN_PROC_BROWSER_TEST_F(SearchAiModePromoTabHelperBrowserTest,
                       AbortsFlowIfRateLimitted) {
  // 1. Navigate active tab to AI page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url_));
  content::WebContents* source_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Assume promo has been already shown 5 times, so it is subject to rate
  // limits.
  Profile* profile =
      Profile::FromBrowserContext(source_contents->GetBrowserContext());
  profile->GetPrefs()->SetInteger(
      prefs::kSearchAIModeSignInPromoShownCountPerProfile, 5);
  ASSERT_FALSE(signin::ShouldShowSearchAIModeSignInPromo(*profile));

  // 2. Open a new tab from the AI page.
  content::WebContentsAddedObserver observer;
  ASSERT_TRUE(content::ExecJs(source_contents,
                              "window.open('" + target_url_.spec() + "')",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  content::WebContents* new_contents = observer.GetWebContents();
  ASSERT_TRUE(new_contents);

  // 3. Verify helper has been created.
  auto* helper = SearchAiModePromoTabHelper::FromWebContents(new_contents);
  ASSERT_TRUE(helper);

  // Ensure navigation is finished and promo logic runs.
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // 4. Verify helper is destroyed.
  EXPECT_FALSE(SearchAiModePromoTabHelper::FromWebContents(new_contents));
}

// Tests that the helper stays alive if the promo is accepted.
IN_PROC_BROWSER_TEST_F(SearchAiModePromoTabHelperBrowserTest,
                       StaysAliveOnPromoAccept) {
  // 1. Navigate active tab to AI page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url_));
  content::WebContents* source_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 2. Open a new tab from the AI page.
  content::WebContentsAddedObserver observer;
  ASSERT_TRUE(content::ExecJs(source_contents,
                              "window.open('" + target_url_.spec() + "')",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  content::WebContents* new_contents = observer.GetWebContents();

  // 3. Verify helper has been created.
  auto* helper = SearchAiModePromoTabHelper::FromWebContents(new_contents);
  ASSERT_TRUE(helper);

  helper->SetSigninPromoControllerFactoryForTesting(base::BindRepeating(
      &SearchAiModePromoTabHelperBrowserTest::CreateMockController,
      base::Unretained(this), /*expected_calls_count=*/1));

  // Ensure navigation is finished and promo logic runs.
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // 4. Simulate accept.
  CHECK(helper->GetSigninPromoControllerForTesting());
  helper->GetSigninPromoControllerForTesting()->HandlePromoClosing(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  // 5. Verify helper is STILL ALIVE.
  EXPECT_TRUE(SearchAiModePromoTabHelper::FromWebContents(new_contents));
}

// Tests that the helper is destroyed if the initiator tab is closed before
// sign-in.
IN_PROC_BROWSER_TEST_F(SearchAiModePromoTabHelperBrowserTest,
                       AbortsFlowIfInitiatorTabIsClosedWhenUserSignsIn) {
  // 1. Navigate active tab to AI page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url_));
  content::WebContents* source_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 2. Open a new tab from the AI page.
  content::WebContentsAddedObserver observer;
  ASSERT_TRUE(content::ExecJs(source_contents,
                              "window.open('" + target_url_.spec() + "')",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  content::WebContents* new_contents = observer.GetWebContents();

  // 3. Verify helper has been created.
  auto* helper = SearchAiModePromoTabHelper::FromWebContents(new_contents);
  ASSERT_TRUE(helper);

  helper->SetSigninPromoControllerFactoryForTesting(base::BindRepeating(
      &SearchAiModePromoTabHelperBrowserTest::CreateMockController,
      base::Unretained(this), /*expected_calls_count=*/1));

  // Ensure navigation is finished and promo logic runs.
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // 4. Close the initiator tab.
  // The helper will still be alive for now, but when the users interacts
  // with the promo it will detect the initiator's destruction and will be
  // destroyed.
  int source_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(source_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(
      source_index, TabCloseTypes::CLOSE_USER_GESTURE);

  // Verify helper is alive (listening for sign-in).
  ASSERT_TRUE(SearchAiModePromoTabHelper::FromWebContents(new_contents));

  // 5. Simulate sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kSearchAIModeBubble)
          .Build("test@gmail.com"));

  // Verify helper is destroyed because initiator was destroyed.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SearchAiModePromoTabHelper::FromWebContents(new_contents) == nullptr;
  }));
}

// Tests that the helper is destroyed if the user signs in from a different
// access point.
IN_PROC_BROWSER_TEST_F(SearchAiModePromoTabHelperBrowserTest,
                       DestructsOnSigninFromDifferentAccessPoint) {
  // 1. Navigate active tab to AI page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url_));
  content::WebContents* source_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 2. Open a new tab from the AI page.
  content::WebContentsAddedObserver observer;
  ASSERT_TRUE(content::ExecJs(source_contents,
                              "window.open('" + target_url_.spec() + "')",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
  content::WebContents* new_contents = observer.GetWebContents();

  // 3. Verify helper has been created.
  auto* helper = SearchAiModePromoTabHelper::FromWebContents(new_contents);
  ASSERT_TRUE(helper);

  helper->SetSigninPromoControllerFactoryForTesting(base::BindRepeating(
      &SearchAiModePromoTabHelperBrowserTest::CreateMockController,
      base::Unretained(this), /*expected_calls_count=*/1));

  // Ensure navigation is finished and promo logic runs.
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  // 4. Simulate sign-in from a different access point.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kSettings)
          .Build("other@gmail.com"));

  // Verify helper is destroyed.
  EXPECT_FALSE(SearchAiModePromoTabHelper::FromWebContents(new_contents));
}

}  // namespace contextual_tasks
