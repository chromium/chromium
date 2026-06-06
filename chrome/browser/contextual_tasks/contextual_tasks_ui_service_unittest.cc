// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/contextual_tasks/site_exclusion_detail.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/contextual_tasks/public/prefs.h"
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
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

class ContextualTasksUI;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

namespace {

class MockActiveTaskContextProvider : public ActiveTaskContextProvider {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RefreshContext, (), (override));
  MOCK_METHOD(void,
              SetContextualTasksPanelController,
              (ContextualTasksPanelController*),
              (override));
  MOCK_METHOD(void, AddLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, RemoveLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, ClearAllLocalTabUnderlines, (), (override));
};

constexpr char kTestUrl[] = "https://example.com";
constexpr char kAiPageUrl[] = "https://google.com/search?udm=50";
constexpr char kSrpHomepage[] = "https://www.google.com/search";
constexpr char kAimHomepage[] = "https://www.google.com/search?udm=50";
constexpr char kAimHomepageThinking[] = "https://www.google.com/search?nem=143";
constexpr char kSrpShopping[] = "https://www.google.com/search?udm=28&q=query";
constexpr char kSrpUrl[] = "https://google.com/search?q=query";
constexpr char kSignOutUrl[] = "https://accounts.google.com/Logout";
constexpr char kSrpUrlWithLensQuery[] =
    "https://www.google.com/search?lns_mode=un";
constexpr char kLabsUrl[] = "https://labs.google.com/search";

class FakeContextualTasksEligibilityManager
    : public ContextualTasksEligibilityManager {
 public:
  FakeContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service)
      : ContextualTasksEligibilityManager(pref_service,
                                          identity_manager,
                                          aim_eligibility_service) {
    MaybeNotifyEligibilityChanged();
  }
  ~FakeContextualTasksEligibilityManager() override = default;

  void SetIsEligible(bool eligible) {
    is_eligible_ = eligible;
    MaybeNotifyEligibilityChanged();
  }

  bool IsEligibleWithoutIdentity() const override { return is_eligible_; }

 protected:
  bool CalculateEligibility() const override { return is_eligible_; }

 private:
  bool is_eligible_ = true;
};

class MockUiServiceForUrlIntercept : public ContextualTasksUiService {
 public:
  explicit MockUiServiceForUrlIntercept(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      AimEligibilityService* aim_eligibility_service)
      : ContextualTasksUiService(
            profile,
            std::make_unique<NiceMock<MockContextualTasksUiServiceDelegate>>(),
            contextual_tasks_service,
            /*identity_manager=*/nullptr,
            aim_eligibility_service,
            std::make_unique<FakeContextualTasksEligibilityManager>(
                profile->GetPrefs(),
                /*identity_manager=*/nullptr,
                aim_eligibility_service),
            /*cookie_synchronizer=*/nullptr) {}
  ~MockUiServiceForUrlIntercept() override = default;

  FakeContextualTasksEligibilityManager* GetFakeEligibilityManager() {
    return static_cast<FakeContextualTasksEligibilityManager*>(
        GetEligibilityManager());
  }

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
               base::WeakPtr<BrowserWindowInterface> browser,
               const url::Origin& initiator_origin),
              (override));
  MOCK_METHOD(void,
              OnNonThreadNavigationInTab,
              (content::OpenURLParams url_params,
               base::WeakPtr<tabs::TabInterface> tab),
              (override));
  MOCK_METHOD(void,
              OnSearchResultsNavigationInSidePanel,
              (content::OpenURLParams url_params,
               ContextualTasksUIInterface* web_ui_interface),
              (override));
  MOCK_METHOD(bool, IsUrlForPrimaryAccount, (const GURL& url), (override));
  MOCK_METHOD(bool, IsSignedInToBrowserWithValidCredentials, (), (override));
  MOCK_METHOD(void,
              LoadUrlInWebContents,
              (const GURL& url,
               base::WeakPtr<content::WebContents> web_contents),
              (override));
  MOCK_METHOD(void,
              OpenUrl,
              (const content::OpenURLParams& url_params,
               const blink::mojom::WindowFeatures& window_features),
              (override));

  using ContextualTasksUiService::HandleNavigationImpl;
  bool HandleNavigationImpl(
      content::OpenURLParams url_params,
      content::WebContents* source_contents,
      tabs::TabInterface* tab,
      bool is_from_embedded_page,
      bool from_can_create_window,
      bool is_same_site_or_from_ui,
      bool is_mobile_ua,
      const std::optional<url::Origin>& initiator_origin,
      const std::optional<content::GlobalRenderFrameHostToken>&
          initiator_frame_token,
      const blink::mojom::WindowFeatures& window_features) override {
    return ContextualTasksUiService::HandleNavigationImpl(
        std::move(url_params), source_contents, tab, is_from_embedded_page,
        from_can_create_window, is_same_site_or_from_ui, is_mobile_ua,
        initiator_origin, initiator_frame_token, window_features);
  }
};

content::OpenURLParams CreateOpenUrlParams(
    const GURL& url,
    bool is_renderer_initiated,
    ui::PageTransition page_transition =
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL) {
  content::Referrer referrer;
  return content::OpenURLParams(url, referrer,
                                WindowOpenDisposition::CURRENT_TAB,
                                page_transition, is_renderer_initiated);
}

// A matcher that checks that an OpenURLParams object has the specified URL.
MATCHER_P(OpenURLParamsHasUrl, expected_url, "") {
  return arg.url == expected_url;
}

}  // namespace
using contextual_tasks::CreateOpenUrlParams;

using contextual_tasks::kAiPageUrl;
using contextual_tasks::kTaskQueryParam;
using contextual_tasks::kTestUrl;
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
        profile_.get(),
        std::make_unique<NiceMock<MockContextualTasksUiServiceDelegate>>(),
        contextual_tasks_service_.get(), identity_test_env_->identity_manager(),
        aim_eligibility_service_.get(),
        std::make_unique<ContextualTasksEligibilityManager>(
            profile_->GetPrefs(), identity_test_env_->identity_manager(),
            aim_eligibility_service_.get()),
        /*cookie_synchronizer=*/nullptr);

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
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

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
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  // The service should NOT retry.
  EXPECT_EQ(token_future.Get(), "");
}

TEST_P(ContextualTasksUiServiceTestParameterized,
       HandleNavigation_NewTabAllowed_TracksWindow_Timeout) {
  if (GetParam() == base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {
    GTEST_SKIP() << "Timeout won't work on SYSTEM_TIME";
  }

  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  ContextualTaskId task_id(base::Uuid::GenerateRandomV4());
  GURL source_url =
      net::AppendQueryParameter(host_web_content_url, kTaskQueryParam,
                                task_id.value().AsLowercaseString());
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(source_url);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  const auto& trackers = service_for_nav_->window_trackers_for_testing();
  ASSERT_EQ(1U, trackers.size());
  EXPECT_EQ(task_id, trackers[0]->task_id());

  // Fast forward time by 10 seconds.
  task_environment()->FastForwardBy(base::Seconds(10));

  // The tracker should be destroyed.
  EXPECT_EQ(0U, service_for_nav_->window_trackers_for_testing().size());
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

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_AiPage_NotSameSite_UntrustedParamAppended) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .WillOnce([&](const GURL& intercepted_url,
                    base::WeakPtr<tabs::TabInterface> tab, bool is_to_new_tab) {
        std::string value;
        EXPECT_TRUE(net::GetValueForKeyInQuery(intercepted_url, "cru", &value));
        EXPECT_EQ("1", value);
        run_loop.Quit();
      });

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/false, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_AiPage_SameSite_UntrustedParamRemoved) {
  GURL ai_url(kAiPageUrl);
  ai_url = net::AppendOrReplaceQueryParameter(ai_url, "cru", "1");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .WillOnce([&](const GURL& intercepted_url,
                    base::WeakPtr<tabs::TabInterface> tab, bool is_to_new_tab) {
        std::string value;
        EXPECT_FALSE(
            net::GetValueForKeyInQuery(intercepted_url, "cru", &value));
        run_loop.Quit();
      });

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  run_loop.Run();
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_ProceedsWhenMobileUserAgent) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false,
      /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true,
      /*is_mobile_ua=*/true, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_RedirectsWhenMobileUserAgentAndContextualTasksUrl) {
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  base::RunLoop run_loop;
  ON_CALL(*service_for_nav_, LoadUrlInWebContents(_, _))
      .WillByDefault([&](const GURL& url,
                         base::WeakPtr<content::WebContents> web_contents) {
        EXPECT_TRUE(
            url.spec().starts_with(kAiPageUrl) ||
            url.spec().starts_with("https://www.google.com/search?udm=50"));
        run_loop.Quit();
      });

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false,
      /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true,
      /*is_mobile_ua=*/true, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  run_loop.Run();
}
#endif

TEST_F(ContextualTasksUiServiceTest, HandleNavigation_AiPage_DebugParam) {
  GURL ai_url(kAiPageUrl);
  ai_url = net::AppendQueryParameter(ai_url, "deb", "nocobrowse1");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_AiPage_DebugParam_Substring) {
  GURL ai_url(kAiPageUrl);
  // Have the known debug value be a substring of the broader value.
  ai_url = net::AppendQueryParameter(ai_url, "deb", "nocobrowse1moredebug1");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_AiPage_DebugParam_VirtualUrl) {
  GURL virtual_url("chrome://google.com/search?udm=50&q=test&deb=nocobrowse1");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  base::RunLoop run_loop;
  ON_CALL(*service_for_nav_, LoadUrlInWebContents(_, _))
      .WillByDefault([&](const GURL& url,
                         base::WeakPtr<content::WebContents> web_contents) {
        std::string value;
        EXPECT_TRUE(net::GetValueForKeyInQuery(url, "deb", &value));
        EXPECT_EQ("nocobrowse1", value);
        run_loop.Quit();
      });

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(virtual_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, HandleNavigation_AiPage_NcbParam) {
  GURL ai_url(kAiPageUrl);
  ai_url = net::AppendQueryParameter(ai_url, "ncb", "1");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_AiPage_NcbParam_VirtualUrl) {
  GURL virtual_url("chrome://google.com/search?udm=50&q=test&ncb=1");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  base::RunLoop run_loop;
  ON_CALL(*service_for_nav_, LoadUrlInWebContents(_, _))
      .WillByDefault([&](const GURL& url,
                         base::WeakPtr<content::WebContents> web_contents) {
        EXPECT_EQ(url.spec(),
                  "https://www.google.com/search?udm=50&q=test&ncb=1");
        run_loop.Quit();
      });

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(virtual_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  run_loop.Run();
}

// A link from the embedded page should be intercepted so that it navigates the
// current tab by default. Initiating cobrowse now requires a message from the
// embedded page.
TEST_F(ContextualTasksUiServiceTest, LinkFromWebUiIntercepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAimTriggeredThreadLinks);

  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_,
              OnNonThreadNavigationInTab(OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, BrowserUiNavigationFromWebUiIgnored) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  // Specifically flag the navigation as not from in-page. This mimics actions
  // like back, forward, and omnibox navigation.
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(GURL(kTestUrl), true), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_NewTabAllowed_TracksWindow) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  ContextualTaskId task_id(base::Uuid::GenerateRandomV4());
  GURL source_url =
      net::AppendQueryParameter(host_web_content_url, kTaskQueryParam,
                                task_id.value().AsLowercaseString());
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(source_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  const auto& trackers = service_for_nav_->window_trackers_for_testing();
  ASSERT_EQ(1U, trackers.size());
  EXPECT_EQ(task_id, trackers[0]->task_id());
  EXPECT_EQ(navigated_url, trackers[0]->expected_url());

  auto* tracker = trackers[0].get();
  auto new_web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(new_web_contents.get())
      ->NavigateAndCommit(navigated_url);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(new_web_contents.get(),
                                                       &mock_tab);
  tabs::TabInterface::WillDetach detach_callback;
  EXPECT_CALL(mock_tab, RegisterWillDetach(_))
      .WillOnce([&](tabs::TabInterface::WillDetach callback) {
        detach_callback = std::move(callback);
        return base::CallbackListSubscription();
      });

  tracker->SetTabWebContents(new_web_contents.get());

  ASSERT_FALSE(detach_callback.is_null());
  detach_callback.Run(&mock_tab, tabs::TabInterface::DetachReason::kDelete);
  // Verify destruction of tracker via callback

  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(0U, service_for_nav_->window_trackers_for_testing().size());
}


TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_NewTabAllowed_TracksWindow_TabListDestroyed) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  ContextualTaskId task_id(base::Uuid::GenerateRandomV4());
  GURL source_url =
      net::AppendQueryParameter(host_web_content_url, kTaskQueryParam,
                                task_id.value().AsLowercaseString());
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(source_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  const auto& trackers = service_for_nav_->window_trackers_for_testing();
  ASSERT_EQ(1U, trackers.size());

  auto* tracker = trackers[0].get();
  auto new_web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(new_web_contents.get())
      ->NavigateAndCommit(navigated_url);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(new_web_contents.get(),
                                                       &mock_tab);
  tabs::TabInterface::WillDetach detach_callback;
  EXPECT_CALL(mock_tab, RegisterWillDetach(_))
      .WillOnce([&](tabs::TabInterface::WillDetach callback) {
        detach_callback = std::move(callback);
        return base::CallbackListSubscription();
      });

  tracker->SetTabWebContents(new_web_contents.get());

  ASSERT_FALSE(detach_callback.is_null());
  detach_callback.Run(&mock_tab, tabs::TabInterface::DetachReason::kDelete);
  // Verify destruction of tracker via callback

  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(0U, service_for_nav_->window_trackers_for_testing().size());
}

TEST_F(ContextualTasksUiServiceTest, AiHostNotIntercepted_BadPath) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("https://google.com/maps?udm=50"));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(GURL(kTestUrl), false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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

  service_for_nav_->GetFakeEligibilityManager()->SetIsEligible(false);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromOmnibox) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL());

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_AlreadyViewingUiInTab) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(ai_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  run_loop.Run();
}

// The AI page is allowed to load as long as it is part of the WebUI.
TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted) {
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(webui_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(GURL(kAiPageUrl), false), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_,
              OnNonThreadNavigationInTab(OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));
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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_,
              OnNonThreadNavigationInTab(OpenURLParamsHasUrl(navigated_url), _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));

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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_,
              OnNonThreadNavigationInTab(OpenURLParamsHasUrl(navigated_url), _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, Navigation_ToNewTab_Allowed) {
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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnNonThreadNavigationInTab(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  content::Referrer referrer;
  content::OpenURLParams params(navigated_url, referrer,
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, true);
  EXPECT_FALSE(service_for_nav_->HandleNavigationImpl(
      std::move(params), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Any other link that isn't AI or an allowed host should be treated as a thread
// link when viewed in a tab.
TEST_F(ContextualTasksUiServiceTest, Navigation_ViewedInTab) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAimTriggeredThreadLinks);

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

  EXPECT_CALL(*service_for_nav_,
              OnNonThreadNavigationInTab(OpenURLParamsHasUrl(navigated_url), _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// A link that is clicked from the side panel and doesn't specify opening in a
// new tab should open in a new tab anyway to avoid navigating the side panel.
// This case represents a likely bug in the embedded page.
TEST_F(ContextualTasksUiServiceTest, Navigation_ViewedInSidePanel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAimTriggeredThreadLinks);

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

  EXPECT_CALL(
      *service_for_nav_,
      OpenUrl(testing::Field(&content::OpenURLParams::url, navigated_url),
              testing::_))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));

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
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));
  run_loop.Run();
}

// If the navigating to the Search Labs page, the navigation should be
// intercepted but open in a new tab.
TEST_F(ContextualTasksUiServiceTest,
       LabsNavigation_Intercepted_NotViewedInSidePanel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAimTriggeredThreadLinks);

  GURL navigated_url(kLabsUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, OnNavigationToAiPageIntercepted_SameTab) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
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
      "https://google.com/search?udm=50&q=test+query&sourceid=chrome&ccb=1");
  EXPECT_EQ(service.GetInitialUrlForTask(task.GetTaskId()),
            expected_initial_url);
}

TEST_F(ContextualTasksUiServiceTest,
       OnNavigationToAiPageIntercepted_PreservesCsParam) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
  GURL intercepted_url("https://google.com/search?udm=50&q=test+query&cs=1");
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
      "https://google.com/"
      "search?udm=50&q=test+query&cs=1&sourceid=chrome&ccb=1");
  EXPECT_EQ(service.GetInitialUrlForTask(task.GetTaskId()),
            expected_initial_url);
}
TEST_F(ContextualTasksUiServiceTest,
       GetContextualTaskUrlForTask_WithEntryPoint) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
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

TEST_F(ContextualTasksUiServiceTest,
       GetContextualTaskUrlForTask_WithHostOverride) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL intercepted_url("https://gws-prod.corp.google.com/search?udm=50&q=test");

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));
  base::WeakPtrFactory weak_factory(&tab);

  ContextualTask task(task_id);
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));
  EXPECT_CALL(*contextual_tasks_service_, AssociateTabWithTask(_, _))
      .Times(testing::AnyNumber());

  // Simulate the interception to populate the map.
  service.OnNavigationToAiPageIntercepted(intercepted_url,
                                          weak_factory.GetWeakPtr(), false);

  // Get the URL and verify it contains the host parameter.
  GURL url = service.GetContextualTaskUrlForTask(task_id);
  std::string host_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kChromeHostParam, &host_value));
  EXPECT_EQ(host_value, "gws-prod.corp.google.com");
}

TEST_F(ContextualTasksUiServiceTest,
       GetContextualTaskUrlForTask_WithDefaultHost_NoForcedHost) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL intercepted_url("https://google.com/search?udm=50&q=test");

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));
  base::WeakPtrFactory weak_factory(&tab);

  ContextualTask task(task_id);
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));
  EXPECT_CALL(*contextual_tasks_service_, AssociateTabWithTask(_, _))
      .Times(testing::AnyNumber());

  // Simulate the interception to populate the map.
  service.OnNavigationToAiPageIntercepted(intercepted_url,
                                          weak_factory.GetWeakPtr(), false);

  // Get the URL and verify it does NOT contain the host parameter.
  GURL url = service.GetContextualTaskUrlForTask(task_id);
  std::string host_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, kChromeHostParam, &host_value));
}

TEST_F(ContextualTasksUiServiceTest, SrpHomepage_Intercepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAimTriggeredThreadLinks);

  GURL navigated_url(kSrpHomepage);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(nav_url, false), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, SrpShoppingMode_InSidePanel_Intercepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAimTriggeredThreadLinks);

  GURL navigated_url(kSrpShopping);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
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

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(nav_url, false), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

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
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
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
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(
                                     OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, GetInitialUrlForTask_HasSourceId) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
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
  std::string ccb;
  EXPECT_TRUE(net::GetValueForKeyInQuery(*initial_url, "ccb", &ccb));
  EXPECT_EQ(ccb, "1");
}

TEST_F(ContextualTasksUiServiceTest, GetDefaultAiPageUrl_HasSourceIdAndCcb) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
  GURL url = service.GetDefaultAiPageUrl();

  std::string sourceid;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "sourceid", &sourceid));
  EXPECT_EQ(sourceid, "chrome");
  std::string ccb;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ccb", &ccb));
  EXPECT_EQ(ccb, "1");
}

TEST_F(ContextualTasksUiServiceTest, ShareUrl_FromEmbeddedPage_Intercepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAimTriggeredThreadLinks);

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
              OpenUrl(testing::Field(
                          &content::OpenURLParams::url,
                          GURL("https://google.com/"
                               "search?q=https%3A%2F%2Fshare.google%2Faimode")),
                      testing::_))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, CopyParamsFromWebUIUrl) {
  GURL base_url("https://google.com/search");
  GURL webui_url("chrome://contextual-tasks?param1=1&param2=2");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1&param2=2"),
      ContextualTasksUiService::CopyParamsFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest, CopyParamsFromWebUIUrl_DuplicateParams) {
  // The value from the webui url should be prioritized and replace existing
  // values on the base url.
  GURL base_url("https://google.com/search?param1=bad");
  GURL webui_url("chrome://contextual-tasks?param1=1&param2=2");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1&param2=2"),
      ContextualTasksUiService::CopyParamsFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest,
       CopyParamsFromWebUIUrl_ParamEncodingCorrect) {
  // Transfer of params from the webui url should not have extra artifacts added
  // and should be decoded correctly prior to being moved to the base url.
  GURL base_url("https://google.com/search?param1=bad");
  GURL webui_url("chrome://contextual-tasks?param1=a+query+with+spaces");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=a+query+with+spaces"),
      ContextualTasksUiService::CopyParamsFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest, GetAiUrlFromWebUIUrl) {
  GURL base_url("https://google.com/search");
  GURL webui_url("chrome://contextual-tasks?param1=1&param2=2");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1&param2=2"),
      ContextualTasksUiService::GetAiUrlFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest, GetAiUrlFromWebUIUrl_HostOverride) {
  GURL base_url("https://google.com/search");
  GURL webui_url(
      "chrome://"
      "contextual-tasks?param1=1&chrome_host=gws-prod.corp.google.com");

  EXPECT_EQ(
      GURL("https://gws-prod.corp.google.com/search?param1=1"),
      ContextualTasksUiService::GetAiUrlFromWebUIUrl(base_url, webui_url));
}

// If the navigation is to sign the user out, ensure it opens outside the
// webview to ensure the user is signed out of the main storage partition.
TEST_F(ContextualTasksUiServiceTest, SignOutNavigation_OpenedInTab) {
  GURL navigated_url(kSignOutUrl);
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
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_,
              OnNonThreadNavigationInTab(OpenURLParamsHasUrl(navigated_url), _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), &tab,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest, ForcedEmbeddedPageHostOverride) {
  // By default, there should be no override.
  EXPECT_EQ("", contextual_tasks::GetForcedEmbeddedPageHost());

  // Set an override and verify it's returned.
  contextual_tasks::SetForcedEmbeddedPageHostOverride("test.google.com");
  EXPECT_EQ("test.google.com", contextual_tasks::GetForcedEmbeddedPageHost());

  // Clearing the override should return to the default state.
  contextual_tasks::SetForcedEmbeddedPageHostOverride("");
  EXPECT_EQ("", contextual_tasks::GetForcedEmbeddedPageHost());
}

TEST_F(ContextualTasksUiServiceTest, IsAllowedHost_WithOverride) {
  // Without override, standard domains should be allowed.
  EXPECT_TRUE(
      ContextualTasksUiService::IsAllowedHost(GURL("https://google.com")));
  EXPECT_TRUE(
      ContextualTasksUiService::IsAllowedHost(GURL("https://www.google.com")));

  // Set an override to a specific testing domain.
  contextual_tasks::SetForcedEmbeddedPageHostOverride("test.c.googlers.com");

  // The override domain should now be allowed.
  EXPECT_TRUE(ContextualTasksUiService::IsAllowedHost(
      GURL("https://test.c.googlers.com")));

  // The standard domains should still be allowed.
  EXPECT_TRUE(
      ContextualTasksUiService::IsAllowedHost(GURL("https://google.com")));
}

TEST_F(ContextualTasksUiServiceTest, IsAllowedHost_LensDebugNotAllowed) {
  EXPECT_FALSE(ContextualTasksUiService::IsAllowedHost(
      GURL("https://lndb.corp.google.com")));
  EXPECT_FALSE(ContextualTasksUiService::IsAllowedHost(
      GURL("https://lndb-autopush.corp.google.com")));
}

TEST_F(ContextualTasksUiServiceTest, HandleNavigation_DisplayUrlRewritten) {
  GURL display_url("chrome://google.com/search?udm=50&q=test+query");
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
        EXPECT_EQ(url.query(), display_url.query());
      });

  // Simulate navigation to the virtual URL.
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(display_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Enter cobrowse if it's forward navigation and is originally from link
// click.
TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_ForwardButtonEnterCobrowseOnLink) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("chrome://contextual-tasks"));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  web_contents->GetController().GoBack();
  content::WebContentsTester::For(web_contents.get())
      ->CommitPendingNavigation();
  web_contents->GetController().GoForward();

  base::RunLoop run_loop;
  GURL navigated_url("https://example.com");
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(
          navigated_url, false,
          ui::PageTransitionFromInt(
              ui::PageTransition::PAGE_TRANSITION_LINK |
              ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK)),
      web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  run_loop.Run();
}

// Do not enter cobrowse if it's forward navigation and is originally from
// typed.
TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_ForwardButtonNotEnterCobrowseOnType) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("chrome://contextual-tasks"));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  web_contents->GetController().GoBack();
  content::WebContentsTester::For(web_contents.get())
      ->CommitPendingNavigation();
  web_contents->GetController().GoForward();

  GURL navigated_url("https://example.com");
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(
          navigated_url, false,
          ui::PageTransitionFromInt(
              ui::PageTransition::PAGE_TRANSITION_TYPED |
              ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK)),
      web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
}

// Do not enter cobrowse if it's back navigation, even if originally from link
// click.
TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_BackButtonNotEnterCobrowseOnLink) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("chrome://contextual-tasks"));
  web_contents->GetController().GoBack();

  GURL navigated_url("https://example.com");
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(
          navigated_url, false,
          ui::PageTransitionFromInt(
              ui::PageTransition::PAGE_TRANSITION_LINK |
              ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK)),
      web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
}

#if !BUILDFLAG(IS_ANDROID)
// Intercept navigation to contextual tasks URL on back/forward navigation
// if kContextualTasksBackButtonExpandsSidePanel is enabled.
TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_BackButtonExpandsSidePanel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kContextualTasksBackButtonExpandsSidePanel);

  tabs::TabModel::PreventFeatureInitializationForTesting prevent_feature_init;

  NiceMock<MockBrowserWindowInterface> mock_browser_window;
  ON_CALL(mock_browser_window, GetProfile())
      .WillByDefault(Return(profile_.get()));
  ON_CALL(testing::Const(mock_browser_window), GetProfile())
      .WillByDefault(Return(profile_.get()));
  ui::UnownedUserDataHost unowned_user_data_host;
  ON_CALL(mock_browser_window, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(unowned_user_data_host));

  NiceMock<MockTabListInterface> mock_tab_list;
  auto tab_list_registration =
      std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
          unowned_user_data_host, mock_tab_list);

  TestTabStripModelDelegate delegate;
  delegate.SetBrowserWindowInterface(&mock_browser_window);
  TabStripModel tab_strip_model(&delegate, profile_.get());
  ON_CALL(mock_browser_window, GetTabStripModel())
      .WillByDefault(Return(&tab_strip_model));

  NiceMock<MockActiveTaskContextProvider> mock_active_task_context_provider;
  auto mock_panel_host =
      std::make_unique<NiceMock<MockContextualTasksPanelHost>>();
  ON_CALL(*mock_panel_host, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));
  ON_CALL(*mock_panel_host, IsPanelInitialized()).WillByDefault(Return(true));

  // Create side panel web contents and set it on the mock panel host.
  auto side_panel_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(side_panel_contents.get())
      ->NavigateAndCommit(GURL("chrome://contextual-tasks"));
  mock_panel_host->SetWebContents(side_panel_contents.get());

  auto coordinator = std::make_unique<ContextualTasksSidePanelCoordinator>(
      &mock_browser_window, std::move(mock_panel_host),
      &mock_active_task_context_provider, nullptr);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  GURL original_tab_url("https://example.com");
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(original_tab_url);

  // The WebContents must be added to the tab strip model to get a valid index.
  tab_strip_model.AppendWebContents(std::move(web_contents), true);
  EXPECT_EQ(tab_strip_model.count(), 1);
  tabs::TabInterface* tab = tab_strip_model.GetTabAtIndex(0);

  GURL navigated_url("chrome://contextual-tasks");

  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(
          navigated_url, false,
          ui::PageTransitionFromInt(
              ui::PageTransition::PAGE_TRANSITION_LINK |
              ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK)),
      tab->GetContents(), tab,
      /*is_from_embedded_page=*/false,
      /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Verify that the tab was closed as part of expanding the side panel.
  EXPECT_EQ(tab_strip_model.count(), 0);

  // Verify that the side panel's web contents navigation controller
  // has the restored URL in the forward history.
  content::NavigationController& controller =
      side_panel_contents->GetController();
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(),
            GURL("chrome://contextual-tasks/"));
}
#endif

class MockCookieSynchronizer : public ContextualTasksCookieSynchronizer {
 public:
  MockCookieSynchronizer(content::BrowserContext* context,
                         signin::IdentityManager* identity_manager)
      : ContextualTasksCookieSynchronizer(context, identity_manager) {}
  MOCK_METHOD(void, CopyCookiesToWebviewStoragePartition, (), (override));
};

TEST_F(ContextualTasksUiServiceTest, EnsureCookiesSynced) {
  auto mock_synchronizer =
      std::make_unique<MockCookieSynchronizer>(profile_.get(), nullptr);
  MockCookieSynchronizer* mock_ptr = mock_synchronizer.get();

  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      std::move(mock_synchronizer));

  EXPECT_CALL(*mock_ptr, CopyCookiesToWebviewStoragePartition()).Times(1);

  service.EnsureCookiesSynced();
}

TEST_F(ContextualTasksUiServiceTest, PrefetchOnEligibilityChange) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       contextual_tasks::kContextualTasksCookiePrefetch},
      {});

  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  base::RepeatingClosure captured_callback;

  EXPECT_CALL(*aim_eligibility_service_, RegisterEligibilityChangedCallback(_))
      .WillOnce([&](base::RepeatingClosure callback) {
        captured_callback = callback;
        return base::CallbackListSubscription();
      });
  EXPECT_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillOnce(Return(false));

  auto mock_synchronizer =
      std::make_unique<MockCookieSynchronizer>(profile_.get(), nullptr);
  MockCookieSynchronizer* mock_ptr = mock_synchronizer.get();

  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      identity_test_env_->identity_manager(), aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), identity_test_env_->identity_manager(),
          aim_eligibility_service_.get()),
      std::move(mock_synchronizer));

  EXPECT_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ptr, CopyCookiesToWebviewStoragePartition()).Times(1);

  captured_callback.Run();
}

TEST_F(ContextualTasksUiServiceTest, PrefetchOnStartupIfAlreadyEligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       contextual_tasks::kContextualTasksCookiePrefetch},
      {});

  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  EXPECT_CALL(*aim_eligibility_service_, RegisterEligibilityChangedCallback(_))
      .WillOnce(Return(base::CallbackListSubscription()));
  EXPECT_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillOnce(Return(true));

  auto mock_synchronizer =
      std::make_unique<MockCookieSynchronizer>(profile_.get(), nullptr);
  MockCookieSynchronizer* mock_ptr = mock_synchronizer.get();

  EXPECT_CALL(*mock_ptr, CopyCookiesToWebviewStoragePartition()).Times(1);

  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      identity_test_env_->identity_manager(), aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), identity_test_env_->identity_manager(),
          aim_eligibility_service_.get()),
      std::move(mock_synchronizer));
}

TEST_F(ContextualTasksUiServiceTest, OnWebUIReady) {
  auto delegate = std::make_unique<MockContextualTasksUiServiceDelegate>();
  auto* delegate_ptr = delegate.get();
  ContextualTasksUiService service(
      profile_.get(), std::move(delegate), contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, /*aim_eligibility_service=*/nullptr,
      /*eligibility_manager=*/nullptr,
      /*cookie_synchronizer=*/nullptr);

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  MockBrowserWindowInterface browser_window_interface;

  EXPECT_CALL(*delegate_ptr, OnWebUIReady(&browser_window_interface, task_id,
                                          web_contents.get()))
      .Times(1);

  service.OnWebUIReady(&browser_window_interface, task_id, web_contents.get());
}
TEST_F(ContextualTasksUiServiceTest, OnWebUIDestroyed) {
  auto delegate = std::make_unique<MockContextualTasksUiServiceDelegate>();
  auto* delegate_ptr = delegate.get();
  ContextualTasksUiService service(
      profile_.get(), std::move(delegate), contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, /*aim_eligibility_service=*/nullptr,
      /*eligibility_manager=*/nullptr,
      /*cookie_synchronizer=*/nullptr);

  std::optional<base::Uuid> task_id = base::Uuid::GenerateRandomV4();
  MockBrowserWindowInterface browser_window;
  EXPECT_CALL(*delegate_ptr, OnWebUIDestroyed(&browser_window, task_id))
      .Times(1);

  service.OnWebUIDestroyed(&browser_window, task_id);
}

TEST_F(ContextualTasksUiServiceTest, RegisterWindow_UpdatesTracker) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  ContextualTaskId task_id(base::Uuid::GenerateRandomV4());
  GURL source_url =
      net::AppendQueryParameter(host_web_content_url, kTaskQueryParam,
                                task_id.value().AsLowercaseString());
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(source_url);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  const auto& trackers = service_for_nav_->window_trackers_for_testing();
  ASSERT_EQ(1U, trackers.size());
  EXPECT_FALSE(trackers[0]->window_id().has_value());

  ContextualWindowId window_id =
      ContextualWindowId(base::UnguessableToken::Create());
  service_for_nav_->RegisterWindow(task_id, navigated_url, window_id);

  EXPECT_TRUE(trackers[0]->window_id().has_value());
  EXPECT_EQ(window_id, trackers[0]->window_id().value());
}

TEST_F(ContextualTasksUiServiceTest, CloseTrackedWindow_ClosesTab) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  ContextualTaskId task_id(base::Uuid::GenerateRandomV4());
  GURL source_url =
      net::AppendQueryParameter(host_web_content_url, kTaskQueryParam,
                                task_id.value().AsLowercaseString());
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(source_url);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  const auto& trackers = service_for_nav_->window_trackers_for_testing();
  ASSERT_EQ(1U, trackers.size());

  ContextualWindowId window_id =
      ContextualWindowId(base::UnguessableToken::Create());
  service_for_nav_->RegisterWindow(task_id, navigated_url, window_id);

  auto* tracker = trackers[0].get();
  auto new_web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(new_web_contents.get())
      ->NavigateAndCommit(navigated_url);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(new_web_contents.get(),
                                                       &mock_tab);
  tabs::TabInterface::WillDetach detach_callback;
  EXPECT_CALL(mock_tab, RegisterWillDetach(_))
      .WillOnce([&](tabs::TabInterface::WillDetach callback) {
        detach_callback = std::move(callback);
        return base::CallbackListSubscription();
      });
  ON_CALL(mock_tab, GetContents).WillByDefault(Return(new_web_contents.get()));

  tracker->SetTabWebContents(new_web_contents.get());

  service_for_nav_->CloseTrackedWindow(window_id);

  // Simulate the guest window being destroyed.
  ASSERT_FALSE(detach_callback.is_null());
  detach_callback.Run(&mock_tab, tabs::TabInterface::DetachReason::kDelete);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service_for_nav_->window_trackers_for_testing().empty();
  }));

  EXPECT_EQ(0U, service_for_nav_->window_trackers_for_testing().size());
}

TEST_F(ContextualTasksUiServiceTest, IsValidUrlForSuggestedTab) {
  SiteExclusionDetail site_exclusion_detail;

  // HTTP / HTTPS urls are valid
  EXPECT_TRUE(IsValidUrlForSuggestedTab(GURL("http://example.com"),
                                        profile_.get(), site_exclusion_detail));
  EXPECT_TRUE(IsValidUrlForSuggestedTab(GURL("https://example.com"),
                                        profile_.get(), site_exclusion_detail));

  // File urls are valid
  EXPECT_TRUE(IsValidUrlForSuggestedTab(GURL("file:///tmp/mock_file.html"),
                                        profile_.get(), site_exclusion_detail));

  // NTP urls are invalid
  EXPECT_FALSE(IsValidUrlForSuggestedTab(
      GURL("chrome://newtab"), profile_.get(), site_exclusion_detail));

  // Internal about:blank urls are invalid
  EXPECT_FALSE(IsValidUrlForSuggestedTab(GURL("about:blank"), profile_.get(),
                                         site_exclusion_detail));
}

TEST_F(ContextualTasksUiServiceTest, SearchResultsLink_HandledAsThreadLink) {
  content::WebContents* outer_contents = web_contents();
  content::WebContentsTester::For(outer_contents)
      ->SetLastCommittedURL(GURL("chrome://contextual-tasks"));
  content::RenderFrameHost* main_frame = outer_contents->GetPrimaryMainFrame();
  ASSERT_NE(main_frame, nullptr);

  // Initialize the main frame tester.
  content::RenderFrameHostTester::For(main_frame)
      ->InitializeRenderFrameIfNeeded();

  // 1. Create a child frame (subframe) in the outer WebContents.
  // Inner WebContents cannot be attached directly to the main frame.
  content::RenderFrameHost* child_frame =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("subframe");
  ASSERT_NE(child_frame, nullptr);
  content::RenderFrameHostTester::For(child_frame)
      ->InitializeRenderFrameIfNeeded();

  // Create the inner WebContents.
  std::unique_ptr<content::WebContents> inner_contents =
      content::WebContentsTester::CreateTestWebContents(
          outer_contents->GetBrowserContext(), nullptr);
  content::WebContentsTester::For(inner_contents.get())
      ->SetLastCommittedURL(GURL(kSrpUrl));

  // Attach the inner WebContents to the child frame.
  outer_contents->AttachInnerWebContents(std::move(inner_contents), child_frame,
                                         /*is_full_page=*/false);

  // Verify that it was successfully attached.
  EXPECT_EQ(outer_contents->GetInnerWebContents().size(), 1u);

  // Test that a navigation from an embedded page that is the SRP is still
  // treated as a thread link.
  GURL navigated_url("http://example.com");
  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(_, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), outer_contents, nullptr,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true,
      /*is_mobile_ua=*/false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  run_loop.Run();
}

}  // namespace contextual_tasks
