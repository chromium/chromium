// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/aim_user_agent_tab_helper.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace contextual_tasks {

class AimUserAgentTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  AimUserAgentTabHelperBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {kContextualTasks, kContextualTasksRearchitecture}, {});
  }

  static std::unique_ptr<KeyedService> CreateMockUiService(
      content::BrowserContext* context) {
    auto mock =
        std::make_unique<testing::NiceMock<MockContextualTasksUiService>>(
            Profile::FromBrowserContext(context), nullptr, nullptr, nullptr,
            nullptr, nullptr);
    ON_CALL(*mock, IsAiUrl(testing::_)).WillByDefault([](const GURL& url) {
      return url.path() == "/ai_page.html";
    });
    ON_CALL(*mock, IsSidePanelOpenAndRequestInSidePanel(testing::_))
        .WillByDefault([](content::WebContents* wc) {
          return wc && wc->GetVisibleURL().path() == "/side_panel_page.html";
        });
    return mock;
  }

  static std::unique_ptr<KeyedService> CreateMockEligibilityService(
      content::BrowserContext* context) {
    auto mock = std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
        *Profile::FromBrowserContext(context)->GetPrefs(), nullptr, nullptr,
        nullptr);
    ON_CALL(*mock, IsCobrowseEligible()).WillByDefault(testing::Return(true));
    return mock;
  }

  MockAimEligibilityService* GetMockAimService() {
    return static_cast<MockAimEligibilityService*>(
        AimEligibilityServiceFactory::GetForProfile(browser()->GetProfile()));
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &AimUserAgentTabHelperBrowserTest::CreateMockUiService));
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &AimUserAgentTabHelperBrowserTest::CreateMockEligibilityService));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&AimUserAgentTabHelperBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto it = request.headers.find("User-Agent");
    if (it != request.headers.end()) {
      last_user_agent_header_ = it->second;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content("<html><body>AIM UA Test Page</body></html>");
    return response;
  }

  std::string last_user_agent_header_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperBrowserTest, TabHelperAttached) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(nullptr, AimUserAgentTabHelper::FromWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperBrowserTest,
                       ApplyAndClearUserAgentOverride) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  AimUserAgentTabHelper* tab_helper =
      AimUserAgentTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  std::string expected_suffix = GetContextualTasksUserAgentSuffix();
  ASSERT_FALSE(expected_suffix.empty());

  // 1. Navigate to an AI Page (/ai_page.html) where IsAiUrl() returns true.
  GURL ai_url = embedded_test_server()->GetURL("/ai_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url));

  // Verify navigator.userAgent in JavaScript contains the UA suffix.
  std::string ai_js_ua =
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString();
  EXPECT_THAT(ai_js_ua, testing::HasSubstr(expected_suffix));

  // Verify the HTTP request header sent over the network contains the UA
  // suffix.
  EXPECT_THAT(last_user_agent_header_, testing::HasSubstr(expected_suffix));

  // 2. Navigate away to a non-AI Page (/non_ai_page.html) where IsAiUrl()
  // returns false.
  GURL non_ai_url = embedded_test_server()->GetURL("/non_ai_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_ai_url));

  // Verify navigator.userAgent in JavaScript NO LONGER contains the UA suffix.
  std::string non_ai_js_ua =
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString();
  EXPECT_THAT(non_ai_js_ua, testing::Not(testing::HasSubstr(expected_suffix)));

  // Verify the HTTP request header sent over the network NO LONGER contains
  // the UA suffix.
  EXPECT_THAT(last_user_agent_header_,
              testing::Not(testing::HasSubstr(expected_suffix)));
}

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperBrowserTest,
                       ApplyUserAgentOverrideInSidePanel) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  AimUserAgentTabHelper* tab_helper =
      AimUserAgentTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  std::string expected_suffix = GetContextualTasksUserAgentSuffix();
  ASSERT_FALSE(expected_suffix.empty());

  // Navigate to a non-AI page in the side panel where
  // IsSidePanelOpenAndRequestInSidePanel() returns true.
  GURL side_panel_url = embedded_test_server()->GetURL("/side_panel_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), side_panel_url));

  // Verify navigator.userAgent in JavaScript contains the UA suffix.
  std::string side_panel_js_ua =
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString();
  EXPECT_THAT(side_panel_js_ua, testing::HasSubstr(expected_suffix));

  // Verify the HTTP request header sent over the network contains the UA
  // suffix.
  EXPECT_THAT(last_user_agent_header_, testing::HasSubstr(expected_suffix));
}

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperBrowserTest,
                       DirectlyCreatedWebContentsUserAgentOverride) {
  // Create a WebContents directly (simulating a side panel WebContents created
  // outside of TabHelpers::AttachTabHelpers).
  content::WebContents::CreateParams create_params(browser()->GetProfile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  // Before explicitly attaching, TabHelpers::AttachTabHelpers was not called,
  // so the helper is null.
  EXPECT_EQ(nullptr,
            AimUserAgentTabHelper::FromWebContents(web_contents.get()));

  // Attach AimUserAgentTabHelper (as done in CreateWebContents for side panel).
  AimUserAgentTabHelper::CreateForWebContents(web_contents.get());
  EXPECT_NE(nullptr,
            AimUserAgentTabHelper::FromWebContents(web_contents.get()));

  std::string expected_suffix = GetContextualTasksUserAgentSuffix();
  ASSERT_FALSE(expected_suffix.empty());

  // Navigate to an AI Page.
  GURL ai_url = embedded_test_server()->GetURL("/ai_page.html");
  content::NavigationController::LoadURLParams params(ai_url);
  web_contents->GetController().LoadURLWithParams(params);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents.get()));

  // Verify navigator.userAgent in JavaScript contains the UA suffix.
  std::string ai_js_ua =
      content::EvalJs(web_contents.get(), "navigator.userAgent")
          .ExtractString();
  EXPECT_THAT(ai_js_ua, testing::HasSubstr(expected_suffix));
}

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperBrowserTest,
                       ApplyUserAgentOverrideWhenCobrowseIneligible) {
  auto* mock_aim = GetMockAimService();
  ASSERT_TRUE(mock_aim);
  ON_CALL(*mock_aim, IsCobrowseEligible())
      .WillByDefault(testing::Return(false));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  AimUserAgentTabHelper* tab_helper =
      AimUserAgentTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  std::string expected_suffix = GetContextualTasksUserAgentSuffix();
  ASSERT_FALSE(expected_suffix.empty());

  // Navigate to an AI Page (/ai_page.html) when cobrowse ineligible.
  GURL ai_url = embedded_test_server()->GetURL("/ai_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url));

  // Verify navigator.userAgent in JavaScript still contains the UA suffix.
  std::string ai_js_ua =
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString();
  EXPECT_THAT(ai_js_ua, testing::HasSubstr(expected_suffix));

  // Verify the HTTP request header sent over the network contains the UA suffix.
  EXPECT_THAT(last_user_agent_header_, testing::HasSubstr(expected_suffix));
}

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperBrowserTest,
                       DoNotApplyUserAgentOverrideWhenAimIneligible) {
  auto* mock_aim = GetMockAimService();
  ASSERT_TRUE(mock_aim);
  ON_CALL(*mock_aim, IsAimEligible()).WillByDefault(testing::Return(false));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  AimUserAgentTabHelper* tab_helper =
      AimUserAgentTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  std::string expected_suffix = GetContextualTasksUserAgentSuffix();
  ASSERT_FALSE(expected_suffix.empty());

  // Navigate to an AI Page (/ai_page.html) when AIM ineligible.
  GURL ai_url = embedded_test_server()->GetURL("/ai_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url));

  // Verify navigator.userAgent in JavaScript does NOT contain the UA suffix.
  std::string ai_js_ua =
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString();
  EXPECT_THAT(ai_js_ua, testing::Not(testing::HasSubstr(expected_suffix)));

  // Verify the HTTP request header sent over the network does NOT contain the
  // UA suffix.
  EXPECT_THAT(last_user_agent_header_,
              testing::Not(testing::HasSubstr(expected_suffix)));
}

class AimUserAgentTabHelperForceCountryCodeUSBrowserTest
    : public AimUserAgentTabHelperBrowserTest {
 public:
  AimUserAgentTabHelperForceCountryCodeUSBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {kContextualTasks, kContextualTasksForceCountryCodeUS,
         kContextualTasksRearchitecture},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperForceCountryCodeUSBrowserTest,
                       ApplyUserAgentOverrideWithForceCountryCodeUS) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  AimUserAgentTabHelper* tab_helper =
      AimUserAgentTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  std::string expected_suffix = GetContextualTasksUserAgentSuffix();
  ASSERT_FALSE(expected_suffix.empty());

  GURL ai_url = embedded_test_server()->GetURL("/ai_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url));

  std::string ai_js_ua =
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString();
  EXPECT_THAT(ai_js_ua, testing::HasSubstr(expected_suffix));
}

class AimUserAgentTabHelperSidePanelOnlyBrowserTest
    : public AimUserAgentTabHelperBrowserTest {
 public:
  AimUserAgentTabHelperSidePanelOnlyBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {kContextualTasksSidePanel, kContextualTasksRearchitecture},
        {kContextualTasks});
  }
};

IN_PROC_BROWSER_TEST_F(AimUserAgentTabHelperSidePanelOnlyBrowserTest,
                       ApplyUserAgentOverrideWithSidePanelOnly) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  AimUserAgentTabHelper* tab_helper =
      AimUserAgentTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, tab_helper);

  std::string expected_suffix = GetContextualTasksUserAgentSuffix();
  ASSERT_FALSE(expected_suffix.empty());

  GURL ai_url = embedded_test_server()->GetURL("/ai_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ai_url));

  std::string ai_js_ua =
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString();
  EXPECT_THAT(ai_js_ua, testing::HasSubstr(expected_suffix));

  EXPECT_THAT(last_user_agent_header_, testing::HasSubstr(expected_suffix));
}

}  // namespace contextual_tasks
