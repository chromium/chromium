// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/test/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

namespace {
constexpr char kGoogleHost[] = "google.com";
constexpr char kSearchAimPath[] = "/search";
constexpr char kSearchResultRelativeUrl[] = "/tea.html";

std::unique_ptr<KeyedService> BuildMockAimServiceEligibilityServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto mock = std::make_unique<MockAimEligibilityService>(
      *profile->GetPrefs(), /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

  EXPECT_CALL(*mock, IsAimEligible()).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock, IsCobrowseEligible())
      .WillRepeatedly(testing::Return(true));
  // Mock HasAimUrlParams to return true for our special AI source page.
  EXPECT_CALL(*mock, HasAimUrlParams(testing::_))
      .WillRepeatedly([&](const GURL& url) {
        return (url.has_path() &&
                url.path().find(kSearchAimPath) != std::string::npos);
      });
  return mock;
}
}  // namespace

class SearchAiModePromoTabHelperInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  SearchAiModePromoTabHelperInteractiveUiTest() {
    signin_promo_feature_list_.InitAndEnableFeature(
        switches::kEnableSearchAIModeSigninPromo);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SearchAiModePromoTabHelperInteractiveUiTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SearchAiModePromoTabHelperInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  // Mocks serving content to:
  // 1) an AI Mode search page that contains a link
  // 2) the page that the search link points to.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
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
    // Replace the real service with a mock one.
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&SearchAiModePromoTabHelperInteractiveUiTest::
                                BuildMockContextualTasksUiService,
                            base::Unretained(this)));
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&BuildMockAimServiceEligibilityServiceInstance));
  }

  std::unique_ptr<KeyedService> BuildMockContextualTasksUiService(
      content::BrowserContext* context) {
    auto mock_ui_service =
        std::make_unique<testing::NiceMock<MockContextualTasksUiService>>();

    // Mock mock IsAiUrl bases on the path of the url.
    ON_CALL(*mock_ui_service, IsAiUrl(testing::_))
        .WillByDefault([](const GURL& url) {
          return url.path().find(kSearchAimPath) != std::string::npos;
        });

    return mock_ui_service;
  }

 private:
  base::test::ScopedFeatureList signin_promo_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(SearchAiModePromoTabHelperInteractiveUiTest,
                       SigninPromoShownForNonSignedInUser) {
  const GURL ai_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchAimPath);
  const GURL result_url =
      embedded_test_server()->GetURL(kGoogleHost, kSearchResultRelativeUrl);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSourceTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);

  RunTestSequence(
      InstrumentTab(kSourceTabId), NavigateWebContents(kSourceTabId, ai_url),
      WaitForElementVisible(kSourceTabId, DeepQuery{"#link"}),
      InstrumentNextTab(kNewTabId),
      ClickElement(kSourceTabId, DeepQuery{"#link"}),
      WaitForWebContentsNavigation(kNewTabId, result_url),
      InAnyContext(WaitForShow(kSearchAIModeSignInPromoFrameViewId)));
}

}  // namespace contextual_tasks
