// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_test_base.h"
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
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/host_override.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
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
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
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

}  // namespace

class ContextualTasksUiServiceTest : public ContextualTasksUiServiceTestBase {
 public:
  explicit ContextualTasksUiServiceTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : ContextualTasksUiServiceTestBase(time_source) {}
};

class ContextualTasksUiServiceTestWithMockTime
    : public ContextualTasksUiServiceTest {
 public:
  ContextualTasksUiServiceTestWithMockTime()
      : ContextualTasksUiServiceTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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
  base::HistogramTester histogram_tester;
  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);
  EXPECT_EQ(token_future.Get(), "");

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Start.All", true,
                                      1);
  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.Start.AimNavigation",
                                    0);
  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Success.All",
                                      false, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.Success.AimNavigation", 0);
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
  // Since we ignore the first 2 errors, this should retry immediately.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  // Second request also fails with a transient error.
  // This should also retry immediately.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  // Third request fails with a transient error.
  // Now we should apply backoff (150ms delay).
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  // The service should retry after delay. Fast forward by less than 150ms
  // first.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(identity_test_env_->IsAccessTokenRequestPending());

  // Fast forward the rest of the way.
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // Fourth request succeeds.
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

#if !BUILDFLAG(IS_ANDROID)
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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
#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksUiServiceTestParameterized,
    testing::Values(base::test::TaskEnvironment::TimeSource::SYSTEM_TIME,
                    base::test::TaskEnvironment::TimeSource::MOCK_TIME));

TEST_F(ContextualTasksUiServiceTest,
       OnNavigationToAiPageIntercepted_TriggersTokenFetch) {
  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

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
  base::WeakPtrFactory weak_factory(&tab);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));
  EXPECT_CALL(*contextual_tasks_service_,
              AssociateTabWithTask(
                  task.GetTaskId(),
                  sessions::SessionTabHelper::IdForTab(web_contents.get())))
      .Times(1);

  real_service_->OnNavigationToAiPageIntercepted(intercepted_url,
                                                 weak_factory.GetWeakPtr(),
                                                 /*is_to_new_tab=*/false);

  EXPECT_TRUE(identity_test_env_->IsAccessTokenRequestPending());
}

TEST_F(ContextualTasksUiServiceTestWithMockTime, OAuthMetrics_AimNavigation) {
  base::HistogramTester histogram_tester;

  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

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
  base::WeakPtrFactory weak_factory(&tab);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));
  EXPECT_CALL(*contextual_tasks_service_,
              AssociateTabWithTask(
                  task.GetTaskId(),
                  sessions::SessionTabHelper::IdForTab(web_contents.get())))
      .Times(1);

  // Trigger early fetch (AimNavigation).
  real_service_->OnNavigationToAiPageIntercepted(intercepted_url,
                                                 weak_factory.GetWeakPtr(),
                                                 /*is_to_new_tab=*/false);

  // Respond with token to complete the fetch.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "fake_token", base::Time::Max());

  // Verify metrics recorded to BOTH All and AimNavigation.
  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Start.All", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.Start.AimNavigation", true, 1);

  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.Latency.All", 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.Latency.AimNavigation", 1);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.TriesCount.All", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.TriesCount.AimNavigation", 1, 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.TriesCountBeforeSuccess.All", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.TriesCountBeforeSuccess.AimNavigation", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeFailure.All", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeFailure.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Success.All", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.Success.AimNavigation", true, 1);

  // Since it was "instant" in mock time, it should be counted as cached.
  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.WasCached.All",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.WasCached.AimNavigation", true, 1);
}

TEST_F(ContextualTasksUiServiceTestWithMockTime, OAuthMetrics_OtherNavigation) {
  base::HistogramTester histogram_tester;

  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  // Trigger standard fetch (Other).
  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);

  // Respond with token.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "fake_token", base::Time::Max());

  EXPECT_EQ(token_future.Get(), "fake_token");

  // Verify metrics recorded to All but NOT AimNavigation.
  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Start.All", true,
                                      1);
  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.Start.AimNavigation",
                                    0);

  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.Latency.All", 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.Latency.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.TriesCount.All", 1,
                                      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCount.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.TriesCountBeforeSuccess.All", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeSuccess.AimNavigation", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeFailure.All", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeFailure.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Success.All", true,
                                      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.Success.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.WasCached.All",
                                      true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.WasCached.AimNavigation", 0);
}

TEST_F(ContextualTasksUiServiceTest, OAuthMetrics_PersistentError) {
  base::HistogramTester histogram_tester;

  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);

  // First request fails with a persistent error.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  EXPECT_EQ(token_future.Get(), "");

  // Verify metrics recorded to All but NOT AimNavigation.
  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.Latency.All", 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.Latency.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.TriesCount.All", 1,
                                      1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCount.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.TriesCountBeforeFailure.All", 1, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeFailure.AimNavigation", 0);

  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeSuccess.All", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeSuccess.AimNavigation", 0);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Success.All",
                                      false, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.Success.AimNavigation", 0);

  // WasCached is only recorded on success.
  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.WasCached.All", 0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.WasCached.AimNavigation", 0);
}

TEST_P(ContextualTasksUiServiceTestParameterized,
       OAuthMetrics_TransientErrorAndSuccess) {
  if (GetParam() == base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {
    GTEST_SKIP() << "Retries won't work on SYSTEM_TIME";
  }

  base::HistogramTester histogram_tester;

  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<const std::string&> token_future;
  real_service_->GetAccessToken(token_future.GetCallback(), nullptr);

  // First try fails with transient error.
  // Retries immediately.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  // Second try fails with transient error.
  // Retries immediately.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  // Third try fails with transient error.
  // Delays 150ms.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  // No metrics recorded yet (still retrying).
  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.Latency.All", 0);

  // Fast forward to trigger retry.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Fourth try succeeds.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "fake_token", base::Time::Max());

  EXPECT_EQ(token_future.Get(), "fake_token");

  // Metrics recorded now.
  histogram_tester.ExpectTotalCount("ContextualTasks.OAuth.Latency.All", 1);

  // Tries count should be 4.
  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.TriesCount.All", 4,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OAuth.TriesCountBeforeSuccess.All", 4, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OAuth.TriesCountBeforeFailure.All", 0);

  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.Success.All", true,
                                      1);

  // Since it took some time (>150ms backoff + network time), it might NOT be
  // counted as cached. In our test, we fast-forwarded by 200ms, which is >
  // 50ms, so WasCached should be FALSE.
  histogram_tester.ExpectUniqueSample("ContextualTasks.OAuth.WasCached.All",
                                      false, 1);
}

TEST_F(ContextualTasksUiServiceTest, IsGoogleCaptchaUrl) {
  ON_CALL(*aim_eligibility_service_, IsAimHost(_, _))
      .WillByDefault(
          [](const GURL& url,
             std::optional<contextual_tasks::HostOverride> host_override) {
            return url.host().find(".google.com") != std::string::npos;
          });

  EXPECT_TRUE(service_for_nav_->IsGoogleCaptchaUrl(
      GURL("https://www.google.com/sorry/index?continue=foo")));
  EXPECT_TRUE(service_for_nav_->IsGoogleCaptchaUrl(
      GURL("https://ipv4.google.com/sorry/index?continue=foo")));
  EXPECT_FALSE(service_for_nav_->IsGoogleCaptchaUrl(
      GURL("https://www.google.com/search?q=test")));
  EXPECT_FALSE(service_for_nav_->IsGoogleCaptchaUrl(
      GURL("https://example.com/sorry/index")));
}

TEST_F(ContextualTasksUiServiceTest, IsAiUrl) {
  // Mock IsAimUrl to return false for everything.
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

  // g.ai should be recognized as AI URL.
  EXPECT_TRUE(service_for_nav_->IsAiUrl(GURL("https://g.ai/")));
  EXPECT_TRUE(service_for_nav_->IsAiUrl(GURL("https://www.g.ai/")));
  EXPECT_TRUE(service_for_nav_->IsAiUrl(GURL("http://g.ai/path")));

  // Non-HTTP/HTTPS URLs or invalid URLs should return false.
  EXPECT_FALSE(service_for_nav_->IsAiUrl(GURL("chrome://g.ai/")));
  EXPECT_FALSE(service_for_nav_->IsAiUrl(GURL("file://g.ai/")));
  EXPECT_FALSE(service_for_nav_->IsAiUrl(GURL()));

  // Other URLs should fall back to mock service (which returns false).
  EXPECT_FALSE(service_for_nav_->IsAiUrl(GURL("https://example.com/")));
  EXPECT_FALSE(service_for_nav_->IsAiUrl(GURL("https://google.com/")));

  // When IsAimUrl returns true for valid URLs:
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(true));
  EXPECT_TRUE(service_for_nav_->IsAiUrl(GURL("https://example.com/")));
  EXPECT_TRUE(service_for_nav_->IsAiUrl(GURL("https://google.com/")));
  // But invalid/non-HTTP/HTTPS URLs should still return false for g.ai.
  EXPECT_FALSE(service_for_nav_->IsAiUrl(GURL("chrome://g.ai/")));
  EXPECT_FALSE(service_for_nav_->IsAiUrl(GURL()));
}

TEST_F(ContextualTasksUiServiceTest, HandleNavigation_AiPage_ChecksCobrowse) {
  GURL ai_url(kAiPageUrl);
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(true));

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

TEST_F(
    ContextualTasksUiServiceTest,
    HandleNavigation_AiPage_CobrowseIneligible_NoAttachedTab_NotIntercepted) {
  base::test::ScopedFeatureList scoped_feature_list(kContextualTasksSidePanel);
  GURL ai_url(kAiPageUrl);
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(true));
  ON_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_AiPage_CobrowseIneligible_WithAttachedTab_Intercepted) {
  base::test::ScopedFeatureList scoped_feature_list(kContextualTasksSidePanel);
  GURL ai_url(kAiPageUrl);
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(true));
  ON_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  sessions::SessionTabHelper::CreateForWebContents(
      web_contents.get(),
      base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));

  auto mock_session = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  contextual_search::FileInfo file_info;
  file_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(web_contents.get());
  std::vector<contextual_search::FileInfo> submitted_files = {file_info};
  ON_CALL(*mock_session, GetSubmittedContextFileInfos)
      .WillByDefault(Return(submitted_files));

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  helper->SetTaskSession(std::nullopt, std::move(mock_session),
                         /*input_state_model=*/nullptr);

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

TEST_F(
    ContextualTasksUiServiceTest,
    HandleNavigation_AiPage_CobrowseIneligible_WithDifferentTabInContext_NotIntercepted) {
  base::test::ScopedFeatureList scoped_feature_list(kContextualTasksSidePanel);
  GURL ai_url(kAiPageUrl);
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(true));
  ON_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  sessions::SessionTabHelper::CreateForWebContents(
      web_contents.get(),
      base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));

  auto mock_session = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  contextual_search::FileInfo file_info;
  // Use a different tab ID than the active tab's ID.
  file_info.tab_session_id = SessionID::FromSerializedValue(9999);
  std::vector<contextual_search::FileInfo> submitted_files = {file_info};
  ON_CALL(*mock_session, GetSubmittedContextFileInfos)
      .WillByDefault(Return(submitted_files));

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  helper->SetTaskSession(std::nullopt, std::move(mock_session),
                         /*input_state_model=*/nullptr);

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_BypassedWhenRearchitectureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      contextual_tasks::kContextualTasksRearchitecture);

  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
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

  ON_CALL(*aim_eligibility_service_, HasNoCobrowseParams(_))
      .WillByDefault(Return(true));

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

  ON_CALL(*aim_eligibility_service_, HasNoCobrowseParams(_))
      .WillByDefault(Return(true));

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
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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

#if !BUILDFLAG(IS_ANDROID)
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksWindowTracking)) {
    EXPECT_TRUE(service_for_nav_->window_trackers_for_testing().empty());
    return;
  }

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
       HandleNavigation_RedirectsSubsequentGuestNavigations) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      contextual_tasks::kContextualTasksWindowTracking);

  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  ContextualTaskId task_id(base::Uuid::GenerateRandomV4());
  GURL source_url =
      net::AppendQueryParameter(host_web_content_url, kTaskQueryParam,
                                task_id.value().AsLowercaseString());
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(source_url);

  content::FrameTreeNodeId guest_frame_tree_node_id =
      content::FrameTreeNodeId::FromUnsafeValue(42);
  content::OpenURLParams open_params = CreateOpenUrlParams(navigated_url, true);
  open_params.frame_tree_node_id = guest_frame_tree_node_id;

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      open_params, web_contents.get(),
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  const auto& trackers = service_for_nav_->window_trackers_for_testing();
  ASSERT_EQ(1U, trackers.size());
  ContextualTasksWindowTracker* tracker = trackers[0].get();
  EXPECT_EQ(guest_frame_tree_node_id, tracker->GetWebViewFrameTreeNodeId());

  auto tab_web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  tracker->SetTabWebContents(tab_web_contents.get());

  GURL guest_nav_url("https://example.com/guest-nav");
  content::OpenURLParams guest_nav_params =
      CreateOpenUrlParams(guest_nav_url, true);
  guest_nav_params.frame_tree_node_id = guest_frame_tree_node_id;
  guest_nav_params.initiator_origin = url::Origin::Create(navigated_url);

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      guest_nav_params, web_contents.get(),
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    content::NavigationEntry* entry =
        tab_web_contents->GetController().GetPendingEntry();
    return entry && entry->GetURL() == guest_nav_url;
  }));
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksWindowTracking)) {
    EXPECT_TRUE(service_for_nav_->window_trackers_for_testing().empty());
    return;
  }

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
#endif

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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
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

#if !BUILDFLAG(IS_ANDROID)
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
#endif

// Any other link that isn't AI or an allowed host should be treated as a thread
// link when viewed in a tab.
TEST_F(ContextualTasksUiServiceTest, Navigation_ViewedInTab) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAimTriggeredThreadLinks);

  GURL navigated_url("https://example.com");
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));
  ON_CALL(*aim_eligibility_service_, IsAimHost(_, _))
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
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
              testing::_, testing::_))
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

TEST_F(ContextualTasksUiServiceTest, CaptchaNavigation_ViewedInSidePanel) {
  GURL navigated_url("https://www.google.com/sorry/index?continue=foo");
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(host_web_content_url);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnSearchResultsNavigationInSidePanel(_, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigationImpl(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(), nullptr,
      /*is_from_embedded_page=*/true,
      /*from_can_create_window=*/false, /*is_same_site_or_from_ui=*/true, false,
      std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));
}

TEST_F(ContextualTasksUiServiceTest, OnNavigationToAiPageIntercepted_SameTab) {
  base::UserActionTester user_action_tester;
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

  EXPECT_EQ(
      user_action_tester.GetActionCount("ContextualTasks.AimFullTab.Shown"), 1);

  GURL expected_initial_url(
      "https://google.com/search?udm=50&q=test+query&sourceid=chrome&ccb=1");
  EXPECT_EQ(service.GetInitialUrlForTask(task.GetTaskId()),
            expected_initial_url);
}

TEST_F(ContextualTasksUiServiceTest,
       OnNavigationToAiPageIntercepted_TransfersSessionAndInputStateModel) {
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

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  auto mock_session = std::make_unique<
      testing::NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  contextual_search::ContextualSearchMetricsRecorder metrics_recorder(
      contextual_search::ContextualSearchSource::kOmnibox);
  ON_CALL(*mock_session, GetMetricsRecorder)
      .WillByDefault(Return(&metrics_recorder));
  mock_session->set_smart_tab_sharing_active(true);

  auto input_state_model = std::make_unique<contextual_search::InputStateModel>(
      *mock_session, omnibox::SearchboxConfig(), GURL(), false, false, false);
  input_state_model->SetSmartTabSharingActive(true);
  std::vector<int32_t> selected_tabs = {123, 456};
  helper->SetTaskSession(std::nullopt, std::move(mock_session),
                         std::move(input_state_model), selected_tabs);

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

  auto* task_session = helper->GetSessionForTask(task.GetTaskId());
  ASSERT_TRUE(task_session);
  EXPECT_TRUE(task_session->smart_tab_sharing_active().value_or(false));
  auto taken_input_state = helper->TakeInputStateModelForTask(task.GetTaskId());
  ASSERT_TRUE(taken_input_state);
  EXPECT_TRUE(taken_input_state->IsSmartTabSharingActive());
  EXPECT_EQ(helper->GetSelectedTabIds(), selected_tabs);
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

TEST_F(ContextualTasksUiServiceTest,
       GetContextualTaskUrlForTask_WithUntrustedHost) {
  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      /*identity_manager=*/nullptr, aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), /*identity_manager=*/nullptr,
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL intercepted_url(
      "https://google.com/"
      "search?udm=50&q=test&chrome_host=malicious.example.com");

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

  // Get the URL and verify it does NOT contain the untrusted host parameter.
  GURL url = service.GetContextualTaskUrlForTask(task_id);
  std::string host_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, kChromeHostParam, &host_value));
}

TEST_F(ContextualTasksUiServiceTest, SrpHomepage_Intercepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAimTriggeredThreadLinks);

  GURL navigated_url(kSrpHomepage);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));
  ON_CALL(*aim_eligibility_service_, IsAimHost(_, _))
      .WillByDefault(Return(true));
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
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
                      testing::_, testing::_))
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

TEST_F(ContextualTasksUiServiceTest, CopyParamsFromWebUIUrl_WithFragment) {
  GURL base_url("https://google.com/search");
  GURL webui_url("chrome://contextual-tasks?param1=1#fragment_value");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1#fragment_value"),
      ContextualTasksUiService::CopyParamsFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest, CopyParamsFromWebUIUrl_ReplaceFragment) {
  GURL base_url("https://google.com/search#old_fragment");
  GURL webui_url("chrome://contextual-tasks?param1=1#new_fragment");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1#new_fragment"),
      ContextualTasksUiService::CopyParamsFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest, CopyParamsFromWebUIUrl_RemoveFragment) {
  GURL base_url("https://google.com/search#old_fragment");
  GURL webui_url("chrome://contextual-tasks?param1=1");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1"),
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

TEST_F(ContextualTasksUiServiceTest,
       GetAiUrlFromWebUIUrl_HostOverrideWithPort) {
  GURL base_url("https://google.com/search");
  GURL webui_url(
      "chrome://"
      "contextual-tasks?param1=1&chrome_host=localhost.corp.google.com:8888");

  EXPECT_EQ(
      GURL("https://localhost.corp.google.com:8888/search?param1=1"),
      ContextualTasksUiService::GetAiUrlFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest,
       GetAiUrlFromWebUIUrl_UntrustedHostOverride) {
  GURL base_url("https://google.com/search");
  GURL webui_url(
      "chrome://"
      "contextual-tasks?param1=1&chrome_host=malicious.example.com");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1"),
      ContextualTasksUiService::GetAiUrlFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest,
       GetAiUrlFromWebUIUrl_BypassAttemptWithSlash) {
  GURL base_url("https://google.com/search");
  GURL webui_url(
      "chrome://"
      "contextual-tasks?param1=1&chrome_host=attacker.com%2F.corp.google.com");

  EXPECT_EQ(
      GURL("https://google.com/search?param1=1"),
      ContextualTasksUiService::GetAiUrlFromWebUIUrl(base_url, webui_url));
}

TEST_F(ContextualTasksUiServiceTest, IsTrustedHost) {
  // Valid trusted hosts
  EXPECT_TRUE(
      ContextualTasksUiService::IsTrustedHost("gws-prod.corp.google.com"));
  EXPECT_TRUE(
      ContextualTasksUiService::IsTrustedHost("GWS-PROD.CORP.GOOGLE.COM"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("corp.google.com"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("test.c.googlers.com"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("c.googlers.com"));
  EXPECT_TRUE(
      ContextualTasksUiService::IsTrustedHost("myproxy.proxy.googlers.com"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("proxy.googlers.com"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("localhost"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("127.0.0.1"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("[::1]"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("::1"));

  // Valid trusted hosts with port
  EXPECT_TRUE(
      ContextualTasksUiService::IsTrustedHost("gws-prod.corp.google.com:8080"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost(
      "localhost.corp.google.com:8888"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("localhost:8080"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("127.0.0.1:8888"));
  EXPECT_TRUE(ContextualTasksUiService::IsTrustedHost("[::1]:8888"));

  // Delimiter and bypass attempts
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("attacker.com/.corp.google.com"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost(
      "attacker.com\\.corp.google.com"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost(
      "attacker.com@gws-prod.corp.google.com"));
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("attacker.com?.corp.google.com"));
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("attacker.com#.corp.google.com"));
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("attacker.com:.corp.google.com"));
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("attacker.com .corp.google.com"));

  // Domain boundary and near-domain bypass attempts
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost("evilcorp.google.com"));
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("evilcorp.google.com:8080"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost("notcorp.google.com"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost("corp0google.com"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost("corp-google.com"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost(
      "gws-prod.corp.google.com.evil.com"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost(".corp.google.com"));
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("malicious.example.com"));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost(""));
  EXPECT_FALSE(ContextualTasksUiService::IsTrustedHost(
      "gws-prod.corp.google.com:99999"));
  EXPECT_FALSE(
      ContextualTasksUiService::IsTrustedHost("gws-prod.corp.google.com:0"));
}

TEST_F(ContextualTasksUiServiceTest, GetHostFromUrl) {
  EXPECT_EQ("gws-prod.corp.google.com",
            ContextualTasksUiService::GetHostFromUrl(GURL(
                "https://google.com?chrome_host=gws-prod.corp.google.com")));
  EXPECT_EQ("gws-prod.corp.google.com",
            ContextualTasksUiService::GetHostFromUrl(GURL(
                "https://google.com?chrome_host=GWS-PROD.CORP.GOOGLE.COM")));
  EXPECT_EQ("test.c.googlers.com",
            ContextualTasksUiService::GetHostFromUrl(
                GURL("https://google.com?chrome_host=test.c.googlers.com")));
  EXPECT_EQ("myproxy.proxy.googlers.com",
            ContextualTasksUiService::GetHostFromUrl(GURL(
                "https://google.com?chrome_host=myproxy.proxy.googlers.com")));
  EXPECT_EQ("localhost", ContextualTasksUiService::GetHostFromUrl(
                             GURL("https://google.com?chrome_host=localhost")));
  EXPECT_EQ("127.0.0.1", ContextualTasksUiService::GetHostFromUrl(
                             GURL("https://google.com?chrome_host=127.0.0.1")));
  EXPECT_EQ("[::1]", ContextualTasksUiService::GetHostFromUrl(
                         GURL("https://google.com?chrome_host=%5B%3A%3A1%5D")));
  EXPECT_EQ("localhost.corp.google.com:8888",
            ContextualTasksUiService::GetHostFromUrl(
                GURL("https://google.com?"
                     "chrome_host=localhost.corp.google.com:8888")));
  EXPECT_EQ("[::1]:8888",
            ContextualTasksUiService::GetHostFromUrl(
                GURL("https://google.com?chrome_host=%5B%3A%3A1%5D:8888")));

  // Bypasses using URL encoding / delimiters
  EXPECT_EQ(std::nullopt, ContextualTasksUiService::GetHostFromUrl(GURL(
                              "https://google.com?"
                              "chrome_host=attacker.com%2F.corp.google.com")));
  EXPECT_EQ(std::nullopt, ContextualTasksUiService::GetHostFromUrl(GURL(
                              "https://google.com?"
                              "chrome_host=attacker.com%5C.corp.google.com")));
  EXPECT_EQ(std::nullopt,
            ContextualTasksUiService::GetHostFromUrl(
                GURL("https://google.com?"
                     "chrome_host=attacker.com@gws-prod.corp.google.com")));
  EXPECT_EQ(std::nullopt,
            ContextualTasksUiService::GetHostFromUrl(
                GURL("https://google.com?chrome_host=evilcorp.google.com")));
  EXPECT_EQ(std::nullopt,
            ContextualTasksUiService::GetHostFromUrl(GURL(
                "https://google.com?chrome_host=corp.google.com.evil.com")));
  EXPECT_EQ(std::nullopt,
            ContextualTasksUiService::GetHostFromUrl(
                GURL("https://google.com?chrome_host=malicious.example.com")));
  EXPECT_EQ(std::nullopt, ContextualTasksUiService::GetHostFromUrl(
                              GURL("https://google.com?other_param=test")));
}

// If the navigation is to sign the user out, ensure it opens outside the
// webview to ensure the user is signed out of the main storage partition.
TEST_F(ContextualTasksUiServiceTest, SignOutNavigation_OpenedInTab) {
  GURL navigated_url(kSignOutUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
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
  EXPECT_FALSE(contextual_tasks::GetForcedEmbeddedPageHost().has_value());

  // Set an override and verify it's returned.
  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"test.google.com", std::nullopt});
  EXPECT_EQ((contextual_tasks::HostOverride{"test.google.com", std::nullopt}),
            contextual_tasks::GetForcedEmbeddedPageHost());

  // Set an override with port and verify it's returned.
  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"localhost.corp.google.com", 8888});
  EXPECT_EQ((contextual_tasks::HostOverride{"localhost.corp.google.com", 8888}),
            contextual_tasks::GetForcedEmbeddedPageHost());

  // Clearing the override should return to the default state.
  contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  EXPECT_FALSE(contextual_tasks::GetForcedEmbeddedPageHost().has_value());
}

TEST_F(ContextualTasksUiServiceTest,
       AddRequiredSidePanelUrlChanges_WithHostOverride) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"test.google.com", std::nullopt});

  GURL url("https://www.google.com/search?q=test");
  GURL new_url = ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
      url, web_contents.get());

  EXPECT_EQ("test.google.com", new_url.host());
  std::string gsc_val;
  EXPECT_TRUE(net::GetValueForKeyInQuery(new_url, "gsc", &gsc_val));
  EXPECT_EQ("2", gsc_val);
  std::string q_val;
  EXPECT_TRUE(net::GetValueForKeyInQuery(new_url, "q", &q_val));
  EXPECT_EQ("test", q_val);

  contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
}

TEST_F(ContextualTasksUiServiceTest,
       AddRequiredSidePanelUrlChanges_WithHostOverrideAndPort) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"localhost.corp.google.com", 8888});

  GURL url("https://www.google.com/search?q=test");
  GURL new_url = ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
      url, web_contents.get());

  EXPECT_EQ("localhost.corp.google.com", new_url.host());
  EXPECT_EQ(8888, new_url.EffectiveIntPort());
  std::string gsc_val;
  EXPECT_TRUE(net::GetValueForKeyInQuery(new_url, "gsc", &gsc_val));
  EXPECT_EQ("2", gsc_val);
  std::string q_val;
  EXPECT_TRUE(net::GetValueForKeyInQuery(new_url, "q", &q_val));
  EXPECT_EQ("test", q_val);

  // Navigating to already-rewritten URL does not trigger further changes (no
  // loop).
  EXPECT_EQ(new_url, ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
                         new_url, web_contents.get()));

  contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
}

TEST_F(ContextualTasksUiServiceTest,
       AddRequiredSidePanelUrlChanges_SignInDomain_NotOverridden) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"test.google.com", std::nullopt});

  GURL signin_url("https://login.corp.google.com/signin");
  GURL new_url = ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
      signin_url, web_contents.get());

  EXPECT_EQ("login.corp.google.com", new_url.host());

  contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
}

TEST_F(ContextualTasksUiServiceTest,
       AddRequiredSidePanelUrlChanges_NonHttpOrHttps_NotOverridden) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"test.google.com", std::nullopt});

  GURL webui_url("chrome://contextual-tasks/?chrome_task_id=123");
  GURL new_url = ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
      webui_url, web_contents.get());

  EXPECT_EQ(webui_url, new_url);

  contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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

  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

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
  EXPECT_FALSE(controller.NeedsReload());
  EXPECT_FALSE(side_panel_contents->IsLoading());

  // Verify that the back-button navigation metric was recorded.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.BackButton.UserAction."
                   "NavigatedFromSidePanelToFullTab"));
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.BackButton.UserAction.NavigatedFromSidePanelToFullTab",
      true, 1);
}
#endif

class MockCookieSynchronizer : public ContextualTasksCookieSynchronizer {
 public:
  MockCookieSynchronizer(content::BrowserContext* context,
                         signin::IdentityManager* identity_manager)
      : ContextualTasksCookieSynchronizer(context, identity_manager) {}
  MOCK_METHOD(void,
              CopyCookiesToWebviewStoragePartition,
              (base::OnceClosure callback),
              (override));
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

  EXPECT_CALL(*mock_ptr, CopyCookiesToWebviewStoragePartition(testing::_))
      .Times(1);

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
      {{.email = std::string(account_info.GetEmail()),
        .gaia_id = account_info.GetGaiaId()}});

  base::RepeatingClosure captured_callback;

  EXPECT_CALL(*aim_eligibility_service_, RegisterEligibilityChangedCallback(_))
      .WillOnce([&](base::RepeatingClosure callback) {
        captured_callback = callback;
        return base::CallbackListSubscription();
      });
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
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

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ptr, CopyCookiesToWebviewStoragePartition(testing::_))
      .Times(1);

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
      {{.email = std::string(account_info.GetEmail()),
        .gaia_id = account_info.GetGaiaId()}});

  EXPECT_CALL(*aim_eligibility_service_, RegisterEligibilityChangedCallback(_))
      .WillOnce(Return(base::CallbackListSubscription()));
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(true));

  auto mock_synchronizer =
      std::make_unique<MockCookieSynchronizer>(profile_.get(), nullptr);
  MockCookieSynchronizer* mock_ptr = mock_synchronizer.get();

  EXPECT_CALL(*mock_ptr, CopyCookiesToWebviewStoragePartition(testing::_))
      .Times(1);

  ContextualTasksUiService service(
      profile_.get(), /*delegate=*/nullptr, contextual_tasks_service_.get(),
      identity_test_env_->identity_manager(), aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), identity_test_env_->identity_manager(),
          aim_eligibility_service_.get()),
      std::move(mock_synchronizer));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_AiPage_CobrowseNotEligible_NotIntercepted) {
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_CALL(*service_for_nav_, LoadUrlInWebContents(_, _)).Times(0);

  // Should return false to allow normal navigation to the AI page.
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(ai_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_WebUI_NotEligible_Redirects) {
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  service_for_nav_->GetFakeEligibilityManager()->SetIsEligible(false);

  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));

  // Run the message loop to allow the navigation to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    content::NavigationEntry* entry =
        web_contents->GetController().GetPendingEntry();
    return entry && entry->GetURL().host() == "www.google.com";
  }));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_WebUI_AimNotEligible_Redirects) {
  base::test::ScopedFeatureList scoped_feature_list(
      contextual_tasks::kContextualTasks);
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  identity_test_env_->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(real_service_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));

  // Run the message loop to allow the navigation to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    content::NavigationEntry* entry =
        web_contents->GetController().GetPendingEntry();
    return entry && entry->GetURL().host() == "www.google.com";
  }));
}

TEST_F(ContextualTasksUiServiceTest, HandleNavigation_WebUI_TokensNotLoaded) {
  base::test::ScopedFeatureList scoped_feature_list(
      contextual_tasks::kContextualTasks);
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(true));

  identity_test_env_->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_->ResetToAccountsNotYetLoadedFromDiskState();
  EXPECT_FALSE(real_service_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(), false, false,
      true, false, std::nullopt, std::nullopt, blink::mojom::WindowFeatures()));
}

TEST_F(
    ContextualTasksUiServiceTest,
    HandleNavigation_WebUI_AimNotEligible_NoRedirect_WhenLensSessionUnderUnification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       lens::features::kLensSidePanelUnification},
      {});
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  identity_test_env_->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(false));

  // Create and associate a Lens-initiated contextual search session.
  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(contextual_search_service);
  auto session_handle = contextual_search_service->CreateSession(
      contextual_tasks::CreateQueryControllerConfigParams(),
      contextual_search::ContextualSearchSource::kLens,
      /*invocation_source=*/std::nullopt);
  ASSERT_TRUE(session_handle);

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  helper->SetTaskSession(std::nullopt, std::move(session_handle),
                         /*input_state_model=*/nullptr);

  // The navigation should not be redirected, so HandleNavigation should return
  // false.
  EXPECT_FALSE(real_service_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));
}

TEST_F(
    ContextualTasksUiServiceTest,
    HandleNavigation_WebUI_AimNotEligible_NoRedirect_WhenPendingLensSessionUnderUnification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       lens::features::kLensSidePanelUnification},
      {});
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL webui_url =
      net::AppendQueryParameter(GURL(chrome::kChromeUIContextualTasksURL),
                                kTaskQueryParam, task_id.AsLowercaseString());
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  identity_test_env_->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(false));

  // Create a Lens-initiated contextual search session.
  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(contextual_search_service);
  auto session_handle = contextual_search_service->CreateSession(
      contextual_tasks::CreateQueryControllerConfigParams(),
      contextual_search::ContextualSearchSource::kLens,
      /*invocation_source=*/std::nullopt);
  ASSERT_TRUE(session_handle);

  // Store in pending_session_handles_ instead of helper.
  real_service_->AddPendingSessionHandleForTesting(task_id,
                                                   std::move(session_handle));

  // The navigation should not be redirected, so HandleNavigation should return
  // false.
  EXPECT_FALSE(real_service_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));
}

TEST_F(
    ContextualTasksUiServiceTest,
    HandleNavigation_WebUI_AimNotEligible_Redirects_WhenNonLensSessionUnderUnification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {contextual_tasks::kContextualTasks,
       lens::features::kLensSidePanelUnification},
      {});
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  identity_test_env_->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(false));

  // Create and associate a non-Lens initiated contextual search session (e.g.
  // kOmnibox).
  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(contextual_search_service);
  auto session_handle = contextual_search_service->CreateSession(
      contextual_tasks::CreateQueryControllerConfigParams(),
      contextual_search::ContextualSearchSource::kOmnibox,
      /*invocation_source=*/std::nullopt);
  ASSERT_TRUE(session_handle);

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  helper->SetTaskSession(std::nullopt, std::move(session_handle),
                         /*input_state_model=*/nullptr);

  // The navigation should still be redirected, so HandleNavigation should
  // return true.
  EXPECT_TRUE(real_service_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_WebUI_NotSignedIn_Redirects) {
  base::test::ScopedFeatureList scoped_feature_list(
      contextual_tasks::kContextualTasks);
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(real_service_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));

  // Run the message loop to allow the navigation to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    content::NavigationEntry* entry =
        web_contents->GetController().GetPendingEntry();
    return entry && entry->GetURL().host() == "www.google.com";
  }));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_WebUI_DefaultSearchNotGoogle_Redirects) {
  base::test::ScopedFeatureList scoped_feature_list(
      contextual_tasks::kContextualTasks);
  GURL webui_url(chrome::kChromeUIContextualTasksURL);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  identity_test_env_->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_.get());
  TemplateURLData data;
  data.SetShortName(u"NonGoogle");
  data.SetKeyword(u"NonGoogle");
  data.SetURL("https://www.nongoogle.com/search?q={searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(real_service_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));

  // Run the message loop to allow the navigation to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    content::NavigationEntry* entry =
        web_contents->GetController().GetPendingEntry();
    return entry && entry->GetURL().host() == "www.google.com";
  }));
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_WebUI_Internals_NotRedirected) {
  GURL webui_url(std::string(chrome::kChromeUIContextualTasksURL) +
                 "internals");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  service_for_nav_->GetFakeEligibilityManager()->SetIsEligible(false);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(web_contents->GetController().GetPendingEntry(), nullptr);
}

TEST_F(ContextualTasksUiServiceTest,
       HandleNavigation_WebUI_TestLoader_NotRedirected) {
  GURL webui_url(std::string(chrome::kChromeUIContextualTasksURL) +
                 "test_loader.html");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  service_for_nav_->GetFakeEligibilityManager()->SetIsEligible(false);

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(webui_url, false), web_contents.get(),
      /*is_from_embedded_page=*/false, /*from_can_create_window=*/false,
      /*is_same_site_or_from_ui=*/true, /*is_mobile_ua=*/false, std::nullopt,
      std::nullopt, blink::mojom::WindowFeatures()));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(web_contents->GetController().GetPendingEntry(), nullptr);
}

#if !BUILDFLAG(IS_ANDROID)
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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksWindowTracking)) {
    EXPECT_TRUE(service_for_nav_->window_trackers_for_testing().empty());
    return;
  }

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

  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      CreateOpenUrlParams(navigated_url, true), web_contents.get(),
      /*is_from_embedded_page=*/true, /*from_can_create_window=*/true,
      /*is_same_site_or_from_ui=*/true, false, std::nullopt, std::nullopt,
      blink::mojom::WindowFeatures()));
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksWindowTracking)) {
    EXPECT_TRUE(service_for_nav_->window_trackers_for_testing().empty());
    return;
  }

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
#endif

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

  // Ensure the inner contents is not detected as an aim page.
  ON_CALL(*aim_eligibility_service_, IsAimUrl(_, _))
      .WillByDefault(Return(false));

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

TEST_F(ContextualTasksUiServiceTest,
       OnNavigationToAiPageIntercepted_OmniboxSearch_WithAttachedTab) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      omnibox::kWebUIOmniboxAskGAboutThisPage);

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

  // Create mock session handle.
  auto mock_session = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();

  // Create a real metrics recorder with kOmnibox source.
  contextual_search::ContextualSearchMetricsRecorder metrics_recorder(
      contextual_search::ContextualSearchSource::kOmnibox);
  ON_CALL(*mock_session, GetMetricsRecorder)
      .WillByDefault(Return(&metrics_recorder));

  // Mock submitted files to contain a tab.
  contextual_search::FileInfo file_info;
  file_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(web_contents.get());
  std::vector<contextual_search::FileInfo> submitted_files = {file_info};
  ON_CALL(*mock_session, GetSubmittedContextFileInfos)
      .WillByDefault(Return(submitted_files));

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  helper->SetTaskSession(std::nullopt, std::move(mock_session),
                         /*input_state_model=*/nullptr);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));

  base::WeakPtrFactory weak_factory(&tab);
  real_service_->OnNavigationToAiPageIntercepted(intercepted_url,
                                                 weak_factory.GetWeakPtr(), false);

  // Verify that the entry point was set to
  // DESKTOP_CHROME_COBROWSE_OMNIBOX_TAB_SEARCH.
  EXPECT_EQ(
      real_service_->GetInitialEntryPointForTask(task.GetTaskId()),
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_TAB_SEARCH);
}

TEST_F(ContextualTasksUiServiceTest,
       OnNavigationToAiPageIntercepted_OmniboxSearch_NoAttachedTab) {
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

  // Create mock session handle.
  auto mock_session = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();

  // Create a real metrics recorder with kOmnibox source.
  contextual_search::ContextualSearchMetricsRecorder metrics_recorder(
      contextual_search::ContextualSearchSource::kOmnibox);
  ON_CALL(*mock_session, GetMetricsRecorder)
      .WillByDefault(Return(&metrics_recorder));

  // Mock submitted files to be empty (no tab).
  std::vector<contextual_search::FileInfo> submitted_files = {};
  ON_CALL(*mock_session, GetSubmittedContextFileInfos)
      .WillByDefault(Return(submitted_files));

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  helper->SetTaskSession(std::nullopt, std::move(mock_session),
                         /*input_state_model=*/nullptr);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));

  base::WeakPtrFactory weak_factory(&tab);
  real_service_->OnNavigationToAiPageIntercepted(intercepted_url,
                                                 weak_factory.GetWeakPtr(), false);

  // Verify that the entry point remains UNKNOWN.
  EXPECT_EQ(real_service_->GetInitialEntryPointForTask(task.GetTaskId()),
            omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
}

TEST_F(ContextualTasksUiServiceTest,
       OnNavigationToAiPageIntercepted_OmniboxSearch_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      omnibox::kWebUIOmniboxAskGAboutThisPage);

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

  // Create mock session handle.
  auto mock_session = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();

  // Create a real metrics recorder with kOmnibox source.
  contextual_search::ContextualSearchMetricsRecorder metrics_recorder(
      contextual_search::ContextualSearchSource::kOmnibox);
  ON_CALL(*mock_session, GetMetricsRecorder)
      .WillByDefault(Return(&metrics_recorder));

  // Mock submitted files to contain a tab.
  contextual_search::FileInfo file_info;
  file_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(web_contents.get());
  std::vector<contextual_search::FileInfo> submitted_files = {file_info};
  ON_CALL(*mock_session, GetSubmittedContextFileInfos)
      .WillByDefault(Return(submitted_files));

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents.get());
  helper->SetTaskSession(std::nullopt, std::move(mock_session),
                         /*input_state_model=*/nullptr);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));

  base::WeakPtrFactory weak_factory(&tab);
  real_service_->OnNavigationToAiPageIntercepted(intercepted_url,
                                                 weak_factory.GetWeakPtr(), false);

  // Verify that the entry point remains UNKNOWN because the feature is
  // disabled.
  EXPECT_EQ(real_service_->GetInitialEntryPointForTask(task.GetTaskId()),
            omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
}

}  // namespace contextual_tasks
