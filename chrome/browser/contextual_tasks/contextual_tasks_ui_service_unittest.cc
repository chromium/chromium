// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

class ContextualTasksUI;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

namespace {

constexpr char kTestUrl[] = "https://example.com";
constexpr char kAiPageUrl[] = "https://google.com/search?udm=50";
constexpr char kSrpHomepage[] = "https://www.google.com/search";
constexpr char kAimHomepage[] = "https://www.google.com/search?udm=50";
constexpr char kAimHomepageThinking[] = "https://www.google.com/search?nem=143";
constexpr char kSrpShopping[] = "https://www.google.com/search?udm=28&q=query";
constexpr char kSrpUrl[] = "https://google.com/search?q=query";
constexpr char kSrpUrlWithLensQuery[] =
    "https://www.google.com/search?lns_mode=un";
constexpr char kLabsUrl[] = "https://labs.google.com/search";

// A mock ContextualTasksUiService that is specifically used for tests around
// intercepting navigation. Namely the `HandleNavigation` method is the real
// implementation with the events being mocked.
class MockUiServiceForUrlIntercept : public ContextualTasksUiService {
 public:
  explicit MockUiServiceForUrlIntercept(
      contextual_tasks::ContextualTasksService* contextual_tasks_service)
      : ContextualTasksUiService(nullptr, contextual_tasks_service, nullptr) {}
  ~MockUiServiceForUrlIntercept() override = default;

  MOCK_METHOD(void,
              OnNavigationToAiPageIntercepted,
              (const GURL& url,
               base::WeakPtr<tabs::TabInterface> tab,
               bool is_to_new_tab),
              (override));
  MOCK_METHOD(void,
              OnThreadLinkClicked,
              (const GURL& url,
               base::Uuid task_id,
               base::WeakPtr<tabs::TabInterface> tab,
               base::WeakPtr<BrowserWindowInterface> browser),
              (override));
  MOCK_METHOD(void,
              OnSearchResultsNavigationInTab,
              (const GURL& url, base::WeakPtr<tabs::TabInterface> tab),
              (override));
  MOCK_METHOD(void,
              OnSearchResultsNavigationInSidePanel,
              (content::OpenURLParams url_params,
               ContextualTasksUI* webui_controller),
              (override));
  MOCK_METHOD(bool, IsUrlForPrimaryAccount, (const GURL& url), (override));
  MOCK_METHOD(bool, IsSignedInToBrowserWithValidCredentials, (), (override));

  // Make the impl method public for this test.
  bool HandleNavigationImpl(content::OpenURLParams url_params,
                            content::WebContents* source_contents,
                            tabs::TabInterface* tab,
                            bool is_from_embedded_page,
                            bool is_to_new_tab) override {
    return ContextualTasksUiService::HandleNavigationImpl(
        std::move(url_params), source_contents, tab, is_from_embedded_page,
        is_to_new_tab);
  }
};

content::OpenURLParams CreateOpenUrlParams(const GURL& url,
                                           bool is_renderer_initiated) {
  content::Referrer referrer;
  return content::OpenURLParams(
      url, referrer, WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL, is_renderer_initiated);
}

// A matcher that checks that an OpenURLParams object has the specified URL.
MATCHER_P(OpenURLParamsHasUrl, expected_url, "") {
  return arg.url == expected_url;
}

}  // namespace

class ContextualTasksUiServiceTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    contextual_tasks_service_ = std::make_unique<MockContextualTasksService>();
    service_for_nav_ = std::make_unique<MockUiServiceForUrlIntercept>(
        contextual_tasks_service_.get());

    ON_CALL(*service_for_nav_, IsUrlForPrimaryAccount(_))
        .WillByDefault(Return(true));
    ON_CALL(*service_for_nav_, IsSignedInToBrowserWithValidCredentials())
        .WillByDefault(Return(true));

    ON_CALL(*contextual_tasks_service_, GetFeatureEligibility)
        .WillByDefault([]() {
          FeatureEligibility eligibility;
          eligibility.contextual_tasks_enabled = true;
          eligibility.aim_eligible = true;
          eligibility.context_sharing_enabled = true;
          return eligibility;
        });
  }

  void TearDown() override {
    service_for_nav_ = nullptr;
    contextual_tasks_service_ = nullptr;
    profile_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MockUiServiceForUrlIntercept> service_for_nav_;
  std::unique_ptr<MockContextualTasksService> contextual_tasks_service_;
};

TEST_F(ContextualTasksUiServiceTest, IsAiUrl_InvalidUrl) {
  GURL url("http://?a=12345");
  EXPECT_FALSE(url.is_valid());
  EXPECT_FALSE(service_for_nav_->IsAiUrl(url));
}

TEST_F(ContextualTasksUiServiceTest, LinkFromWebUiIntercepted) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, BrowserUiNavigationFromWebUiIgnored) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  // Specifically flag the navigation as not from in-page. This mimics actions
  // like back, forward, and omnibox navigation.
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

// Ensure we're not intercepting a link when it doesn't meet any of our
// conditions.
TEST_F(ContextualTasksUiServiceTest, NormalLinkNotIntercepted) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("https://example.com/foo"));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(GURL(kTestUrl), true), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiHostNotIntercepted_BadPath) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("https://google.com/maps?udm=50"));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(GURL(kTestUrl), false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted_NotEligible) {
  GURL ai_url(kAiPageUrl);
  GURL tab_url(kTestUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(tab_url);

  ON_CALL(*contextual_tasks_service_, GetFeatureEligibility)
      .WillByDefault([]() {
        FeatureEligibility eligibility;
        eligibility.contextual_tasks_enabled = false;
        eligibility.aim_eligible = false;
        eligibility.context_sharing_enabled = false;
        return eligibility;
      });

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

// Verifies the happy path. The AI page is intercepted when the user is signed
// in to Chrome and the web identity matches.
TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromTab) {
  GURL ai_url(kAiPageUrl);
  GURL tab_url(kTestUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(tab_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromOmnibox) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL());

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_AlreadyViewingUiInTab) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(ai_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));
  run_loop.Run();
}

// The AI page is allowed to load as long as it is part of the WebUI.
TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted) {
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(webui_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(GURL(kAiPageUrl), false), web_contents.get(),
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

// If the AI page is for an account other than the primary one in chrome, don't
// intercept the navigation.
TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted_AccountMismatch) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL());

  ON_CALL(*service_for_nav_, IsUrlForPrimaryAccount(_))
      .WillByDefault(Return(false));
  ON_CALL(*service_for_nav_, IsSignedInToBrowserWithValidCredentials())
      .WillByDefault(Return(true));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

// Test for identity case: Browser Identity: Signed out.
// This covers cases where the user is signed out of Chrome, regardless of
// web identity.
TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted_BrowserSignedOut) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL());

  ON_CALL(*service_for_nav_, IsSignedInToBrowserWithValidCredentials())
      .WillByDefault(Return(false));

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));
  task_environment()->RunUntilIdle();
}

// If the search results page is navigated to while viewing the UI in a tab,
// ensure the correct event is fired.
TEST_F(ContextualTasksUiServiceTest, SearchResultsNavigation_ViewedInTab) {
  GURL navigated_url(kSrpUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_,
              OnSearchResultsNavigationInTab(navigated_url, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));
  run_loop.Run();
}

// If the search results page is navigated to while viewing the UI in the side
// panel (e.g. no tab tied to the WebContents), ensure the correct event is
// fired.
TEST_F(ContextualTasksUiServiceTest,
       SearchResultsNavigation_ViewedInSidePanel) {
  GURL navigated_url(kSrpUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));
  run_loop.Run();
}

// If the navigating to the Search Labs page, the navigation should be
// intercepted but open in a new tab.
TEST_F(ContextualTasksUiServiceTest,
       LabsNavigation_Intercepted_NotViewedInSidePanel) {
  GURL navigated_url(kLabsUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, GetThreadUrlFromTaskId) {
  base::Uuid task_id =
      base::Uuid::ParseCaseInsensitive("10000000-0000-0000-0000-000000000000");
  ContextualTask task(task_id);

  const std::string server_id = "1234";
  const std::string title = "title";
  const std::string turn_id = "5678";
  Thread thread(ThreadType::kAiMode, server_id, title, turn_id);

  task.SetTitle(title);
  task.AddThread(thread);

  ON_CALL(*contextual_tasks_service_, GetTaskById)
      .WillByDefault([&](const base::Uuid& task_id,
                         base::OnceCallback<void(std::optional<ContextualTask>)>
                             callback) { std::move(callback).Run(task); });

  base::RunLoop run_loop;
  service_for_nav_->GetThreadUrlFromTaskId(
      task_id, base::BindOnce(
                   [](const std::string& server_id, const std::string& turn_id,
                      GURL url) {
                     std::string mstk;
                     net::GetValueForKeyInQuery(url, "mstk", &mstk);
                     ASSERT_EQ(mstk, turn_id);

                     std::string mtid;
                     net::GetValueForKeyInQuery(url, "mtid", &mtid);
                     ASSERT_EQ(mtid, server_id);
                   },
                   server_id, turn_id)
                   .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, OnNavigationToAiPageIntercepted_SameTab) {
  ContextualTasksUiService service(nullptr, contextual_tasks_service_.get(),
                                   nullptr);
  GURL intercepted_url("https://google.com/search?udm=50&q=test+query");

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  sessions::SessionTabHelper::CreateForWebContents(
      web_contents.get(),
      base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));

  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));
  EXPECT_CALL(*contextual_tasks_service_,
              AssociateTabWithTask(
                  task.GetTaskId(),
                  sessions::SessionTabHelper::IdForTab(web_contents.get())))
      .Times(1);
  base::WeakPtrFactory weak_factory(&tab);

  service.OnNavigationToAiPageIntercepted(intercepted_url,
                                          weak_factory.GetWeakPtr(), false);

  GURL expected_initial_url(
      "https://google.com/search?udm=50&q=test+query&cs=0&gsc=2&hl=en");
  EXPECT_EQ(service.GetInitialUrlForTask(task.GetTaskId()),
            expected_initial_url);
}

TEST_F(ContextualTasksUiServiceTest, SrpHomepage_Intercepted) {
  GURL navigated_url(kSrpHomepage);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, AimHomepage_InTab_NotIntercepted) {
  GURL nav_url(kAimHomepage);
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(webui_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(nav_url, false), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AimHomepage_InSidePanel_Intercepted) {
  GURL navigated_url(kAimHomepage);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, SrpShoppingMode_InSidePanel_Intercepted) {
  GURL navigated_url(kSrpShopping);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, AimHomepageThinking_InTab_NotIntercepted) {
  GURL nav_url(kAimHomepageThinking);
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(webui_url);
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(nav_url, false), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));

  // TODO(crbug.com/470448689): RunUntilIdle is needed to ensure the EXPECT_CALL
  // above that is sent to a posted task never gets called. Using RunUntilIdle
  // is bad practice and these tests should be updated to avoid the need for
  // RunUntilIdle.
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest,
       AimHomepageThinking_InSidePanel_Intercepted) {
  GURL navigated_url(kAimHomepageThinking);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, LensQuery_Intercepted) {
  GURL navigated_url(kSrpUrlWithLensQuery);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));
  run_loop.Run();
}

}  // namespace contextual_tasks
