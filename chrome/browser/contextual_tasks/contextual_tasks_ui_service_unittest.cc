// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      AimEligibilityService* aim_eligibility_service)
      : ContextualTasksUiService(profile,
                                 contextual_tasks_service,
                                 nullptr,
                                 aim_eligibility_service) {}
  ~MockUiServiceForUrlIntercept() override = default;

  MOCK_METHOD(void,
              SetInitialEntryPointForTask,
              (const base::Uuid& task_id,
               omnibox::ChromeAimEntryPoint entry_point),
              (override));
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
              OnNonThreadNavigationInTab,
              (const GURL& url, base::WeakPtr<tabs::TabInterface> tab),
              (override));
  MOCK_METHOD(void,
              OnSearchResultsNavigationInSidePanel,
              (content::OpenURLParams url_params,
               ContextualTasksUIInterface* web_ui_interface),
              (override));
  MOCK_METHOD(void,
              OnShareUrlNavigation,
              (const GURL& url),
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
  explicit ContextualTasksUiServiceTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : content::RenderViewHostTestHarness(time_source) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    // IdentityTestEnvironment must be created after the TaskEnvironment.
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();

    profile_ = std::make_unique<TestingProfile>();
    contextual_tasks_service_ = std::make_unique<MockContextualTasksService>();
    aim_eligibility_service_ = std::make_unique<MockAimEligibilityService>(
        prefs_, nullptr, nullptr, nullptr);

    // By default, assume URLs have the correct URL params to be intercepted.
    ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
        .WillByDefault(Return(true));
    ON_CALL(*aim_eligibility_service_, IsCobrowseEligible())
        .WillByDefault(Return(true));

    service_for_nav_ = std::make_unique<MockUiServiceForUrlIntercept>(
        profile_.get(), contextual_tasks_service_.get(),
        aim_eligibility_service_.get());

    ON_CALL(*service_for_nav_, IsUrlForPrimaryAccount(_))
        .WillByDefault(Return(true));
    ON_CALL(*service_for_nav_, IsSignedInToBrowserWithValidCredentials())
        .WillByDefault(Return(true));

    // Create a real service for testing non-mocked methods like GetAccessToken.
    // We pass the IdentityManager from the test environment.
    real_service_ = std::make_unique<ContextualTasksUiService>(
        profile_.get(), contextual_tasks_service_.get(),
        identity_test_env_->identity_manager(), aim_eligibility_service_.get());

    ON_CALL(*contextual_tasks_service_, GetFeatureEligibility)
        .WillByDefault([]() {
          FeatureEligibility eligibility;
          eligibility.contextual_tasks_enabled = true;
          eligibility.aim_eligible = true;
          eligibility.context_sharing_enabled = true;
          return eligibility;
        });

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile_.get());

    // Set up default search provider.
    TemplateURLData data;
    data.SetShortName(u"TestEngine");
    data.SetKeyword(u"TestEngine");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

    // Ensure template url service is fully loaded before executing any test
    // logic.
    if (!template_url_service->loaded()) {
      base::test::TestFuture<bool> loaded_future;
      base::CallbackListSubscription subscription =
          template_url_service->RegisterOnLoadedCallback(base::BindOnce(
              [](base::test::TestFuture<bool>* future) {
                future->SetValue(true);
              },
              &loaded_future));
      template_url_service->Load();
      ASSERT_TRUE(loaded_future.Get());
    }
  }

  void TearDown() override {
    real_service_ = nullptr;
    service_for_nav_ = nullptr;
    contextual_tasks_service_ = nullptr;
    identity_test_env_.reset();
    profile_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 protected:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<MockAimEligibilityService> aim_eligibility_service_;
  std::unique_ptr<MockUiServiceForUrlIntercept> service_for_nav_;
  std::unique_ptr<ContextualTasksUiService> real_service_;
  std::unique_ptr<MockContextualTasksService> contextual_tasks_service_;
};

class ContextualTasksUiServiceTestParameterized
    : public ContextualTasksUiServiceTest,
      public testing::WithParamInterface<
          base::test::TaskEnvironment::TimeSource> {
 public:
  ContextualTasksUiServiceTestParameterized()
      : ContextualTasksUiServiceTest(GetParam()) {}
};

TEST_P(ContextualTasksUiServiceTestParameterized, GetAccessToken_Success) {
  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);

  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));
  EXPECT_EQ(token_future.Get(), "access_token");
}

TEST_P(ContextualTasksUiServiceTestParameterized, GetAccessToken_NotSignedIn) {
  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);
  EXPECT_EQ(token_future.Get(), "");
}

// TODO(crbug.com/477018818): Flaky on Linux ASan.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_GetAccessToken_TransientError_Retries \
  DISABLED_GetAccessToken_TransientError_Retries
#else
#define MAYBE_GetAccessToken_TransientError_Retries \
  GetAccessToken_TransientError_Retries
#endif
TEST_P(ContextualTasksUiServiceTestParameterized,
       MAYBE_GetAccessToken_TransientError_Retries) {
  if (GetParam() == base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {
    GTEST_SKIP() << "Retries won't work on SYSTEM_TIME";
  }

  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);

  // First request fails with a transient error.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

  // The service should retry. We need to fast forward time to trigger the
  // retry. The backoff policy has an initial delay of 500ms.
  task_environment()->FastForwardBy(base::Milliseconds(1000));

  // Second request succeeds.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_EQ(token_future.Get(), "access_token");
}

TEST_P(ContextualTasksUiServiceTestParameterized,
       GetAccessToken_PersistentError) {
  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);

  // First request fails with a persistent error.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // The service should NOT retry.
  EXPECT_EQ(token_future.Get(), "");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksUiServiceTestParameterized,
    testing::Values(base::test::TaskEnvironment::TimeSource::SYSTEM_TIME,
                    base::test::TaskEnvironment::TimeSource::MOCK_TIME));

TEST_F(ContextualTasksUiServiceTest, IsAiUrl_InvalidUrl) {
  GURL url("http://?a=12345");
  EXPECT_FALSE(url.is_valid());
  EXPECT_FALSE(service_for_nav_->IsAiUrl(url));
}

TEST_F(ContextualTasksUiServiceTest, IsAiUrl_ValidAiUrl) {
  GURL ai_url(kAiPageUrl);
  EXPECT_CALL(*aim_eligibility_service_, HasAimUrlParams(ai_url))
      .WillOnce(Return(true));
  EXPECT_TRUE(service_for_nav_->IsAiUrl(ai_url));
}

TEST_F(ContextualTasksUiServiceTest, IsAiUrl_NoAimUrlParams) {
  GURL ai_url(kAiPageUrl);
  EXPECT_CALL(*aim_eligibility_service_, HasAimUrlParams(ai_url))
      .WillOnce(Return(false));
  EXPECT_FALSE(service_for_nav_->IsAiUrl(ai_url));
}

TEST_F(ContextualTasksUiServiceTest, HandleNavigation_AiPage_ChecksCobrowse) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillOnce(Return(true));
  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  run_loop.Run();
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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// If the search results page is navigated to while viewing the UI in a tab,
// ensure the correct event is fired.
TEST_F(ContextualTasksUiServiceTest, SearchResultsNavigation_ViewedInTab) {
  GURL navigated_url(kSrpUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNonThreadNavigationInTab(navigated_url, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));
  run_loop.Run();
}

// If the search results patch is navigated to by a link in the embedded page
// but doesn't have a query (e.g. search home), make sure it isn't treated as
// a thread link click.
TEST_F(ContextualTasksUiServiceTest,
       SearchResultsNavigation_ViewedInTab_NoQuery) {
  GURL navigated_url(kSrpHomepage);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNonThreadNavigationInTab(navigated_url, _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Any non-AI page navigation when viewed in a tab should navigate the tab.
TEST_F(ContextualTasksUiServiceTest, AllowedHostNavigation_ViewedInTab) {
  GURL navigated_url("https://google.com");
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNonThreadNavigationInTab(navigated_url, _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Any other link that isn't AI or an allowed host should be treated as a thread
// link when viewed in a tab.
TEST_F(ContextualTasksUiServiceTest, Navigation_ViewedInTab) {
  GURL navigated_url("https://example.com");
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _, _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNonThreadNavigationInTab(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*is_to_new_tab=*/false));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
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

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

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
                                   nullptr, aim_eligibility_service_.get());
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
      "https://google.com/search?udm=50&q=test+query&cs=0&gsc=2&hl=en&"
      "sourceid=chrome");
  EXPECT_EQ(service.GetInitialUrlForTask(task.GetTaskId()),
            expected_initial_url);
}

TEST_F(ContextualTasksUiServiceTest,
       GetContextualTaskUrlForTask_WithEntryPoint) {
  ContextualTasksUiService service(nullptr, contextual_tasks_service_.get(),
                                   nullptr, aim_eligibility_service_.get());
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  omnibox::ChromeAimEntryPoint entry_point =
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON;

  // Set the entry point for the task.
  service.SetInitialEntryPointForTask(task_id, entry_point);

  // Get the URL and verify it contains the `aep` and `source` parameter.
  GURL url = service.GetContextualTaskUrlForTask(task_id);
  std::string aep_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "aep", &aep_value));
  EXPECT_EQ(aep_value, base::NumberToString(static_cast<int>(entry_point)));

  std::string source_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "source", &source_value));
  EXPECT_EQ(source_value, "chrome.crn.cct");
}

TEST_F(ContextualTasksUiServiceTest, SrpHomepage_Intercepted) {
  GURL navigated_url(kSrpHomepage);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

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

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
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

TEST_F(ContextualTasksUiServiceTest, GetInitialUrlForTask_HasSourceId) {
  ContextualTasksUiService service(nullptr, contextual_tasks_service_.get(),
                                   nullptr, aim_eligibility_service_.get());
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

  std::optional<GURL> initial_url =
      service.GetInitialUrlForTask(task.GetTaskId());
  ASSERT_TRUE(initial_url.has_value());

  std::string sourceid;
  EXPECT_TRUE(net::GetValueForKeyInQuery(*initial_url, "sourceid", &sourceid));
  EXPECT_EQ(sourceid, "chrome");
}

TEST_F(ContextualTasksUiServiceTest, GetDefaultAiPageUrl_HasSourceId) {
  ContextualTasksUiService service(nullptr, contextual_tasks_service_.get(),
                                   nullptr, aim_eligibility_service_.get());
  GURL url = service.GetDefaultAiPageUrl();

  std::string sourceid;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "sourceid", &sourceid));
  EXPECT_EQ(sourceid, "chrome");
}

TEST_F(ContextualTasksUiServiceTest, ShareUrl_FromEmbeddedPage_Intercepted) {
  GURL navigated_url(
      "https://google.com/"
      "search?q=https%3A%2F%2Fshare.google%2Faimode&gsc=2");
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_,
              OnShareUrlNavigation(GURL(
                  "https://google.com/"
                  "search?q=https%3A%2F%2Fshare.google%2Faimode")))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*is_to_new_tab=*/false));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, GetAimUrlFromContextualTasksUrl) {
  // Search param not found.
  EXPECT_TRUE(ContextualTasksUiService::GetAimUrlFromContextualTasksUrl(
                  GURL("chrome://contextual-tasks"))
                  .is_empty());

  // Not valid AIM URL.
  EXPECT_TRUE(
      ContextualTasksUiService::GetAimUrlFromContextualTasksUrl(
          GURL("chrome://contextual-tasks?aim_url=https%3A%2F%2Fbing.com"))
          .is_empty());

  // Valid AIM URL.
  EXPECT_EQ(GURL("https://google.com/search"),
            ContextualTasksUiService::GetAimUrlFromContextualTasksUrl(GURL(
                "chrome://"
                "contextual-tasks?aim_url=https%3A%2F%2Fgoogle.com%2Fsearch")));
}

TEST_F(ContextualTasksUiServiceTest, HandleNavigation_VirtualUrlRewritten) {
  GURL virtual_url("chrome://googlesearch/?udm=50&q=test+query");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  // Expect that the navigation to the virtual URL is intercepted.
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .WillOnce([&](const GURL& url, base::WeakPtr<tabs::TabInterface> tab,
                    bool is_to_new_tab) {
        // Check that the base URL has been rewritten to the standard AIM Google
        // URL.
        EXPECT_EQ(url.scheme(), "https");
        EXPECT_EQ(url.host(), "www.google.com");
        EXPECT_EQ(url.path(), "/search");

        // Verify that the entire query string is copied verbatim.
        EXPECT_EQ(url.query(), virtual_url.query());
      });

  // Simulate navigation to the virtual URL.
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(virtual_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*is_to_new_tab=*/false));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace contextual_tasks
