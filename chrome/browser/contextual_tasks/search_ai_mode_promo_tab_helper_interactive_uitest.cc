// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/search_ai_mode_promo_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_signin_button_view.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

namespace {
constexpr char kGoogleHost[] = "google.com";
constexpr char kSearchAimPath[] = "/ai";
constexpr char kSearchResultRelativeUrl[] = "/tea.html";
constexpr char kTestEmail[] = "test@gmail.com";
constexpr char kMockContextualTaskUrl[] = "http://google.com/ai";
constexpr char kEmptyResultAIMSearchUrl[] = "http://google.com/ai/empty";
constexpr char kEmptyResultAIMSearchPath[] = "/ai/empty";

std::unique_ptr<KeyedService> BuildMockAimServiceEligibilityServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto aim_eligibility_service = std::make_unique<MockAimEligibilityService>(
      *profile->GetPrefs(), /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

  // Mock HasAimUrlParams to return true for our special AI source page.
  EXPECT_CALL(*aim_eligibility_service, HasAimUrlParams(testing::_))
      .WillRepeatedly([&](const GURL& url) {
        return (url.has_path() &&
                url.path().find(kSearchAimPath) != std::string::npos);
      });
  return aim_eligibility_service;
}
}  // namespace

class SearchAiModePromoTabHelperInteractiveUiTestBase
    : public InteractiveBrowserTest {
 public:
  explicit SearchAiModePromoTabHelperInteractiveUiTestBase(
      bool load_original_aim_search) {
    std::vector<base::test::FeatureRef> enabled_features = {
        switches::kEnableSearchAIModeSigninPromo,
        contextual_tasks::kContextualTasks};
    std::vector<base::test::FeatureRef> disabled_features;

    if (load_original_aim_search) {
      enabled_features.push_back(
          contextual_tasks::kEnableLoadOriginalAIMSearchAfterSigninPromo);
    } else {
      disabled_features.push_back(
          contextual_tasks::kEnableLoadOriginalAIMSearchAfterSigninPromo);
    }
    signin_promo_feature_list_.InitWithFeatures(enabled_features,
                                                disabled_features);
  }

  ~SearchAiModePromoTabHelperInteractiveUiTestBase() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SearchAiModePromoTabHelperInteractiveUiTestBase::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void TearDownOnMainThread() override {
    mock_ui_service_ = nullptr;
    identity_test_env_adaptor_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SearchAiModePromoTabHelperInteractiveUiTestBase::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  // Mocks serving content to:
  // 1) an AI Mode search page that contains a link
  // 2) the page that the search link points to.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Mocked response for an empty AIM search page.
    if (request.relative_url.find(kEmptyResultAIMSearchPath) !=
        std::string::npos) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("<!DOCTYPE html><html><body></body></html>");
      return response;
    }
    // Mocked response for an AIM search page with a url.
    if (request.relative_url.find(kSearchAimPath) != std::string::npos) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(
          "<!DOCTYPE html><html><body>"
          "<a id=\"link\" href=\"/tea.html\" target=\"_blank\">Link Tea</a>"
          "</body></html>");
      return response;
    }
    // Mocked response for an AIM search result.
    if (request.relative_url == kSearchResultRelativeUrl) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(
          "<!DOCTYPE html><html><body>This a page about tea</body></html>");
      return response;
    }
    return nullptr;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    // Replace the real service with a mock one.
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&SearchAiModePromoTabHelperInteractiveUiTestBase::
                                BuildMockContextualTasksUiService,
                            base::Unretained(this)));
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&BuildMockAimServiceEligibilityServiceInstance));
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::unique_ptr<KeyedService> BuildMockContextualTasksUiService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto eligibility_manager =
        std::make_unique<ContextualTasksEligibilityManager>(
            profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
            AimEligibilityServiceFactory::GetForProfile(profile));
    auto mock_ui_service =
        std::make_unique<testing::NiceMock<MockContextualTasksUiService>>(
            profile, ContextualTasksServiceFactory::GetForProfile(profile),
            IdentityManagerFactory::GetForProfile(profile),
            AimEligibilityServiceFactory::GetForProfile(profile),
            std::move(eligibility_manager),
            /*cookie_synchronizer=*/nullptr);
    mock_ui_service_ = mock_ui_service.get();
    CHECK(mock_ui_service_);

    // Mock IsAiUrl based on the path of the url.
    ON_CALL(*mock_ui_service_.get(), IsAiUrl(testing::_))
        .WillByDefault([](const GURL& url) {
          return url.path().find(kSearchAimPath) != std::string::npos;
        });
    ON_CALL(*mock_ui_service_.get(), GetDefaultAiPageUrl()).WillByDefault([]() {
      return GURL(kEmptyResultAIMSearchUrl);
    });
    return mock_ui_service;
  }

  auto SignInFromActiveTab(const std::string& email) {
    return Steps(Do([this, email]() {
      CHECK(mock_ui_service_);
      // After the sign-in mock the mock_ui_service_ methods to
      // recognize a primary account.
      ON_CALL(*mock_ui_service_.get(), IsUrlForPrimaryAccount(testing::_))
          .WillByDefault([](const GURL& url) { return true; });
      ON_CALL(*mock_ui_service_.get(),
              GetThreadUrlFromTaskId(testing::_, testing::_))
          .WillByDefault([](const base::Uuid& task_id,
                            base::OnceCallback<void(GURL)> callback) {
            std::move(callback).Run(GURL(kMockContextualTaskUrl));
          });

      content::WebContents* signin_contents =
          browser()->tab_strip_model()->GetActiveWebContents();
      // Simulate adding the account from the web.
      AccountInfo account_info =
          identity_test_env()->MakeAccountAvailable(email);

      // Mock processing the GAIA sign-in completion.
      // Using the signin_contents ensures the signin-in access point is
      // correctly used.
      std::unique_ptr<ProcessDiceHeaderDelegateImpl>
          process_dice_header_delegate_impl =
              ProcessDiceHeaderDelegateImpl::Create(signin_contents);
      process_dice_header_delegate_impl->CompleteChromeSignInAfterGaiaSignin(
          account_info);
      // Refresh token, otherwise the contextual_task_ui_service will not
      // intercept the post-signin navigation.
      identity_test_env()->SetRefreshTokenForPrimaryAccount();
    }));
  }

 private:
  raw_ptr<MockContextualTasksUiService> mock_ui_service_;
  base::test::ScopedFeatureList signin_promo_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

class SearchAiModePromoTabHelperInteractiveUiTest
    : public SearchAiModePromoTabHelperInteractiveUiTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SearchAiModePromoTabHelperInteractiveUiTest()
      : SearchAiModePromoTabHelperInteractiveUiTestBase(GetParam()) {}

  ~SearchAiModePromoTabHelperInteractiveUiTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SearchAiModePromoTabHelperInteractiveUiTest,
                       SigninPromoShownForNonSignedInUser) {
  const GURL ai_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchAimPath);
  const GURL result_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchResultRelativeUrl);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSourceTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSignInTabId);

  base::HistogramTester histogram_tester;

  RunTestSequence(InAnyContext(
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      InstrumentTab(kSourceTabId), NavigateWebContents(kSourceTabId, ai_url),
      WaitForElementVisible(kSourceTabId, DeepQuery{"#link"}),
      InstrumentNextTab(kNewTabId),
      ClickElement(kSourceTabId, DeepQuery{"#link"}),
      WaitForWebContentsNavigation(kNewTabId, result_url),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2),
      // Promo should be visible for a non-signed-in user initially.
      WaitForShow(kSearchAIModeSignInPromoFrameViewId),
      // Wait for the sign-in button to be ready.
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      // Click the "Sign in" button from the promo.
      InstrumentNextTab(kSignInTabId),
      MoveMouseTo(BubbleSignInPromoSignInButtonView::kPromoSignInButton),
      ClickMouse(),
      // Wait for the sign-in tab to open and the promo to hide.
      WaitForShow(kSignInTabId),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  3),
      WaitForHide(kSearchAIModeSignInPromoFrameViewId),
      // Mimic sign-in completion.
      SignInFromActiveTab(kTestEmail),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  3)));

  histogram_tester.ExpectUniqueSample(
      "Signin.SignInPromo.Accepted",
      signin_metrics::AccessPoint::kSearchAIModeBubble, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::kSearchAIModeBubble, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SearchAiModePromoTabHelperInteractiveUiTest,
                         testing::Bool());

class SearchAiModePromoTabHelperInteractiveBubbleDismissalUiTest
    : public SearchAiModePromoTabHelperInteractiveUiTestBase {
 public:
  SearchAiModePromoTabHelperInteractiveBubbleDismissalUiTest()
      : SearchAiModePromoTabHelperInteractiveUiTestBase(
            /*load_original_aim_search=*/false) {
    self_dismissal_feature_list_.InitAndEnableFeature(
        switches::kSearchAIModeSignInPromoSelfDismissal);
  }

  ~SearchAiModePromoTabHelperInteractiveBubbleDismissalUiTest() override =
      default;

 private:
  base::test::ScopedFeatureList self_dismissal_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SearchAiModePromoTabHelperInteractiveBubbleDismissalUiTest,
    SigninPromoSelfDismissedOnTimeout) {
  const GURL ai_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchAimPath);
  const GURL result_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchResultRelativeUrl);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSourceTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);

  RunTestSequence(InAnyContext(
      InstrumentTab(kSourceTabId), NavigateWebContents(kSourceTabId, ai_url),
      WaitForElementVisible(kSourceTabId, DeepQuery{"#link"}),
      InstrumentNextTab(kNewTabId),
      ClickElement(kSourceTabId, DeepQuery{"#link"}),
      WaitForWebContentsNavigation(kNewTabId, result_url),
      // Promo should be visible for a non-signed-in user initially.
      WaitForShow(kSearchAIModeSignInPromoFrameViewId),
      EnsurePresent(kSearchAIModeSignInPromoViewId),
      WithView(kSearchAIModeSignInPromoViewId,
               [](SearchAIModeSignInPromoView* view) {
                 view->FireTimerForTesting();
               }),
      WaitForHide(kSearchAIModeSignInPromoFrameViewId),
      PollUntil(
          [this]() {
            return SearchAiModePromoTabHelper::FromWebContents(
                       browser()->tab_strip_model()->GetActiveWebContents()) ==
                   nullptr;
          },
          "Wait for tab helper to be destroyed")));
}

IN_PROC_BROWSER_TEST_F(
    SearchAiModePromoTabHelperInteractiveBubbleDismissalUiTest,
    TabHelperDestroyedOnPromoDismissal) {
  const GURL ai_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchAimPath);
  const GURL result_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchResultRelativeUrl);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSourceTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);

  RunTestSequence(InAnyContext(
      InstrumentTab(kSourceTabId), NavigateWebContents(kSourceTabId, ai_url),
      WaitForElementVisible(kSourceTabId, DeepQuery{"#link"}),
      InstrumentNextTab(kNewTabId),
      ClickElement(kSourceTabId, DeepQuery{"#link"}),
      WaitForWebContentsNavigation(kNewTabId, result_url),
      // Promo should be visible for a non-signed-in user initially.
      WaitForShow(kSearchAIModeSignInPromoFrameViewId),
      CheckResult(
          [this]() {
            return SearchAiModePromoTabHelper::FromWebContents(
                       browser()->tab_strip_model()->GetActiveWebContents()) !=
                   nullptr;
          },
          true),
      // Dismiss the promo using the close button.
      PressButton(views::BubbleFrameView::kCloseButtonElementId),
      WaitForHide(kSearchAIModeSignInPromoFrameViewId),
      // Verify the tab helper is destroyed.
      PollUntil(
          [this]() {
            return SearchAiModePromoTabHelper::FromWebContents(
                       browser()->tab_strip_model()->GetActiveWebContents()) ==
                   nullptr;
          },
          "Wait for tab helper destruction")));
}

class SearchAiModePromoTabHelperInteractiveBubbleNoSelfDismissalUiTest
    : public SearchAiModePromoTabHelperInteractiveUiTestBase {
 public:
  SearchAiModePromoTabHelperInteractiveBubbleNoSelfDismissalUiTest()
      : SearchAiModePromoTabHelperInteractiveUiTestBase(
            /*load_original_aim_search=*/true) {
    feature_list_.InitAndDisableFeature(
        switches::kSearchAIModeSignInPromoSelfDismissal);
  }

  ~SearchAiModePromoTabHelperInteractiveBubbleNoSelfDismissalUiTest() override =
      default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SearchAiModePromoTabHelperInteractiveBubbleNoSelfDismissalUiTest,
    SigninPromoNoSelfDismissed) {
  const GURL ai_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchAimPath);
  const GURL result_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchResultRelativeUrl);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSourceTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);

  RunTestSequence(InAnyContext(
      InstrumentTab(kSourceTabId), NavigateWebContents(kSourceTabId, ai_url),
      WaitForElementVisible(kSourceTabId, DeepQuery{"#link"}),
      InstrumentNextTab(kNewTabId),
      ClickElement(kSourceTabId, DeepQuery{"#link"}),
      WaitForWebContentsNavigation(kNewTabId, result_url),
      // Promo should be visible for a non-signed-in user initially.
      WaitForShow(kSearchAIModeSignInPromoFrameViewId),
      EnsurePresent(kSearchAIModeSignInPromoViewId),
      WithView(kSearchAIModeSignInPromoViewId,
               [](SearchAIModeSignInPromoView* view) {
                 EXPECT_FALSE(view->IsTimerRunningForTesting());
               })));
}

}  // namespace contextual_tasks
