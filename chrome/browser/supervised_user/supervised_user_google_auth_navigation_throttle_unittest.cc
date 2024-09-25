// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kExampleURL[] = "http://www.example1.com/123";
constexpr char kGoogleSearchURL[] = "https://www.google.com/search?q=test";
constexpr char kGoogleHomeURL[] = "https://www.google.com";
constexpr char kYoutubeDomain[] = "https://www.youtube.com";
constexpr char kChildTestEmail[] = "child@example.com";

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

std::unique_ptr<KeyedService> CreateMockSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::MockSyncService>();
}

}  // namespace

class SupervisedUserGoogleAuthNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  void SetUserAsSupervised() {
    SetPrimaryAccount(identity_manager(), kChildTestEmail,
                      signin::ConsentLevel::kSignin);
    profile()->SetIsSupervisedProfile();
    ASSERT_TRUE(profile()->IsChild());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                SyncServiceFactory::GetInstance(),
                base::BindRepeating(&CreateMockSyncService)},
            TestingProfile::TestingFactory{
                ChromeSigninClientFactory::GetInstance(),
                base::BindRepeating(&BuildTestSigninClient)}};
  }

  std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
  CreateNavigationThrottle(const GURL& url, bool skip_jni_call = true) {
    handle =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            url, main_rfh());
    std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle> throttle =
        SupervisedUserGoogleAuthNavigationThrottle::MaybeCreate(handle.get());

    if (skip_jni_call) {
      throttle->set_skip_jni_call_for_testing(true);
      throttle->set_cancel_deferred_navigation_callback_for_testing(
          base::BindRepeating(
              [](content::NavigationThrottle::ThrottleCheckResult result) {
                ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
                          result);
              }));
    }
    return throttle;
  }

  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    auto* signin_client = ChromeSigninClientFactory::GetForProfile(profile());
    return static_cast<TestSigninClient*>(signin_client)
        ->GetTestURLLoaderFactory();
  }

 private:
  std::unique_ptr<content::MockNavigationHandle> handle;
};

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest,
       NavigationForValidSignedinSupervisedUsers) {
  SetUserAsSupervised();
#if !BUILDFLAG(IS_ANDROID)
  SetRefreshTokenForPrimaryAccount(identity_manager());
#endif
  signin::SetListAccountsResponseOneAccountWithParams(
      {kChildTestEmail,
       identity_manager()
           ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
           .gaia,
       /* valid = */ true,
       /* is_signed_out = */ false,
       /* verified = */ true},
      GetTestURLLoaderFactory());
  identity_manager()->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();

  // For authenticated supervised users all URIs are available.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest());
  EXPECT_EQ(
      content::NavigationThrottle::PROCEED,
      CreateNavigationThrottle(GURL(kGoogleSearchURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kYoutubeDomain))->WillStartRequest());

  // Prerendering is not supported for supervised users.
  std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
      prenderedThrottle = CreateNavigationThrottle(GURL(kExampleURL));

  EXPECT_CALL(
      *(content::MockNavigationHandle*)prenderedThrottle->navigation_handle(),
      IsInPrerenderedMainFrame())
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            prenderedThrottle->WillStartRequest());
}

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest,
       NavigationForPendingSignedInSupervisedUsers) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      supervised_user::kForceSupervisedUserReauthenticationForYouTube);
#endif
  SetUserAsSupervised();
#if !BUILDFLAG(IS_ANDROID)
  SetInvalidRefreshTokenForPrimaryAccount(
      identity_manager(),
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
#endif  // !BUILDFLAG(IS_ANDROID)
  // An invalid, signed-in account is not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {kChildTestEmail,
       identity_manager()
           ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
           .gaia,
       /* valid = */ false,
       /* is_signed_out = */ false,
       /* verified = */ true},
      GetTestURLLoaderFactory());
  identity_manager()->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();

  // For a supervised account that is in the pending state, navigation to Google
  // and YouTube can be subject to throttling.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // On desktop platforms, non-YouTube navigation are permitted.
  EXPECT_EQ(
      content::NavigationThrottle::PROCEED,
      CreateNavigationThrottle(GURL(kGoogleSearchURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))->WillStartRequest());
  // YouTube navigation is cancelled and accompanied with a re-authentication
  // interstitial.
  content::NavigationThrottle::ThrottleCheckResult youtube_navigation_throttle =
      CreateNavigationThrottle(GURL(kYoutubeDomain))->WillStartRequest();
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            youtube_navigation_throttle.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
            youtube_navigation_throttle.net_error_code());
  EXPECT_NE(std::string::npos,
            youtube_navigation_throttle.error_page_content()->find(
                "supervised-user-verify"));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // For ChromeOS, navigation to Google and YouTube are deferred.
  EXPECT_EQ(
      content::NavigationThrottle::DEFER,
      CreateNavigationThrottle(GURL(kGoogleSearchURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kYoutubeDomain))->WillStartRequest());
#elif BUILDFLAG(IS_ANDROID)
  // For Android, navigation to Google and YouTube are deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleSearchURL), true)
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kYoutubeDomain))->WillStartRequest());
#endif

  // Prerendering is not supported for supervised users.
  std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
      prenderedThrottle = CreateNavigationThrottle(GURL(kExampleURL));

  EXPECT_CALL(
      *(content::MockNavigationHandle*)prenderedThrottle->navigation_handle(),
      IsInPrerenderedMainFrame())
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            prenderedThrottle->WillStartRequest());
}

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest,
       NavigationForNotFreshSupervisedUsers) {
  SetUserAsSupervised();
  signin::SetFreshnessOfAccountsInGaiaCookie(identity_manager(), false);

  // For supervised users that are stale, navigation to Google and
  //  Youtube are deferred.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kExampleURL))->WillStartRequest());
  EXPECT_EQ(
      content::NavigationThrottle::DEFER,
      CreateNavigationThrottle(GURL(kGoogleSearchURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kYoutubeDomain))->WillStartRequest());

  // Prerendering is not supported for supervised users.
  std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
      prenderedThrottle = CreateNavigationThrottle(GURL(kExampleURL));

  EXPECT_CALL(
      *(content::MockNavigationHandle*)prenderedThrottle->navigation_handle(),
      IsInPrerenderedMainFrame())
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            prenderedThrottle->WillStartRequest());
}

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest, NavigationForNonUsers) {
  // Throttling is not required for non supervised accounts.
  EXPECT_EQ(nullptr, CreateNavigationThrottle(GURL(kExampleURL), false));
}
