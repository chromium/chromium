// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"

#include <memory>
#include <string>

#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/child_accounts/list_family_members_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

constexpr char kExampleURL[] = "http://www.example1.com/123";
constexpr char kGoogleSearchURL[] = "https://www.google.com/search?q=test";
constexpr char kGoogleHomeURL[] = "https://www.google.com";
constexpr char kYoutubeDomain[] = "https://www.youtube.com";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
constexpr char kYoutubeAccountsDomain[] = "https://accounts.youtube.com";
#endif

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

class MockNavigationSubframeHandle : public content::MockNavigationHandle {
 public:
  MockNavigationSubframeHandle(const GURL& url,
                               content::RenderFrameHost* render_frame_host)
      : content::MockNavigationHandle(url, render_frame_host) {}
  content::FrameType GetNavigatingFrameType() const override {
    return content::FrameType::kSubframe;
  }
};

class SupervisedUserGoogleAuthNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() final {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    ChildAccountServiceFactory::GetForProfile(profile());
  }

  void TearDown() final {
    subframe_ = nullptr;
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_adaptor_->identity_test_env()->identity_manager();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                 ChromeSigninClientFactory::GetInstance(),
                 base::BindRepeating(&BuildTestSigninClient)},
             // List family members service is also activated for the supervised
             // profiles; at the same time it periodically queries the token
             // service to send backend requests. Such interactions would be
             // difficult and pointless to test in this unit.
             TestingProfile::TestingFactory{
                 ListFamilyMembersServiceFactory::GetInstance(),
                 BrowserContextKeyedServiceFactory::TestingFactory{}}});
  }

  std::unique_ptr<content::MockNavigationThrottleRegistry>
  CreateNavigationThrottle(const GURL& url,
                           bool skip_jni_call = true,
                           bool for_subframe = false) {
    if (for_subframe) {
      content::RenderFrameHostTester::For(main_rfh())
          ->InitializeRenderFrameIfNeeded();
      subframe_ = content::RenderFrameHostTester::For(main_rfh())
                      ->AppendChild("subframe");
      handle_ =
          std::make_unique<::testing::NiceMock<MockNavigationSubframeHandle>>(
              url, subframe_);
    } else {
      handle_ =
          std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
              url, main_rfh());
    }

    auto registry = std::make_unique<content::MockNavigationThrottleRegistry>(
        handle_.get(),
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    SupervisedUserGoogleAuthNavigationThrottle::MaybeCreateAndAdd(
        *registry.get());

    if (skip_jni_call) {
      CHECK_EQ(registry->throttles().size(), 1u);
      raw_ptr<SupervisedUserGoogleAuthNavigationThrottle> throttle =
          static_cast<SupervisedUserGoogleAuthNavigationThrottle*>(
              registry->throttles().back().get());

      throttle->set_skip_jni_call_for_testing(true);
      throttle->set_cancel_deferred_navigation_callback_for_testing(
          base::BindRepeating(
              [](content::NavigationThrottle::ThrottleCheckResult result) {
                ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
                          result);
              }));
    }
    return registry;
  }

  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    auto* signin_client = ChromeSigninClientFactory::GetForProfile(profile());
    return static_cast<TestSigninClient*>(signin_client)
        ->GetTestURLLoaderFactory();
  }

 private:
  std::unique_ptr<content::MockNavigationHandle> handle_;
  raw_ptr<content::RenderFrameHost> subframe_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest,
       NavigationForValidSignedinSupervisedUsers) {
  SupervisedUserTestEnvironment::EnableSupervisedAccount(identity_manager());
#if !BUILDFLAG(IS_ANDROID)
  SetRefreshTokenForPrimaryAccount(identity_manager());
#endif
  CoreAccountInfo account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  signin::SetListAccountsResponseOneAccountWithParams(
      {account_info.email, account_info.gaia,
       /*valid=*/true,
       /*is_signed_out=*/false,
       /*verified=*/true},
      GetTestURLLoaderFactory());
  identity_manager()->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();

  // For authenticated supervised users all URIs are available.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kExampleURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kGoogleSearchURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kYoutubeDomain))
                ->throttles()
                .back()
                ->WillStartRequest());

  // Prerendering is not supported for supervised users.
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GURL(kExampleURL));
  raw_ptr<SupervisedUserGoogleAuthNavigationThrottle> prerendered_throttle =
      static_cast<SupervisedUserGoogleAuthNavigationThrottle*>(
          registry->throttles().back().get());

  EXPECT_CALL(*static_cast<content::MockNavigationHandle*>(
                  prerendered_throttle->navigation_handle()),
              IsInPrerenderedMainFrame())
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            prerendered_throttle->WillStartRequest());
}

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest,
       NavigationForPendingSignedInSupervisedUsers) {
  SupervisedUserTestEnvironment::EnableSupervisedAccount(identity_manager());
#if !BUILDFLAG(IS_ANDROID)
  SetInvalidRefreshTokenForPrimaryAccount(
      identity_manager(),
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
#endif  // !BUILDFLAG(IS_ANDROID)
  // An invalid, signed-in account is not authenticated.
  CoreAccountInfo account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  signin::SetListAccountsResponseOneAccountWithParams(
      {account_info.email, account_info.gaia,
       /*valid=*/false,
       /*is_signed_out=*/false,
       /*verified=*/true},
      GetTestURLLoaderFactory());
  identity_manager()->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();

  // For a supervised account that is in the pending state, navigation to Google
  // and YouTube can be subject to throttling.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kExampleURL))
                ->throttles()
                .back()
                ->WillStartRequest());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // On desktop platforms, non-YouTube navigation are permitted.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kGoogleSearchURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  // YouTube navigation is cancelled and accompanied with a re-authentication
  // interstitial.
  content::NavigationThrottle::ThrottleCheckResult youtube_navigation_throttle =
      CreateNavigationThrottle(GURL(kYoutubeDomain))
          ->throttles()
          .back()
          ->WillStartRequest();
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            youtube_navigation_throttle.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
            youtube_navigation_throttle.net_error_code());
  EXPECT_NE(std::string::npos,
            youtube_navigation_throttle.error_page_content()->find(
                "supervised-user-verify"));
#elif BUILDFLAG(IS_CHROMEOS)
  // For ChromeOS, navigation to Google and YouTube are deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleSearchURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kYoutubeDomain))
                ->throttles()
                .back()
                ->WillStartRequest());
#elif BUILDFLAG(IS_ANDROID)
  // For Android, navigation to Google and YouTube are deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleSearchURL), true)
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kYoutubeDomain))
                ->throttles()
                .back()
                ->WillStartRequest());
#endif

  // Prerendering is not supported for supervised users.
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GURL(kExampleURL));
  raw_ptr<SupervisedUserGoogleAuthNavigationThrottle> prerendered_throttle =
      static_cast<SupervisedUserGoogleAuthNavigationThrottle*>(
          registry->throttles().back().get());

  EXPECT_CALL(*static_cast<content::MockNavigationHandle*>(
                  prerendered_throttle->navigation_handle()),
              IsInPrerenderedMainFrame())
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            prerendered_throttle->WillStartRequest());
}

// In order to correctly perform authentication to youtube.com, its
// infrastructure (accounts.youtube.com) must be allowed.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(
    SupervisedUserGoogleAuthNavigationThrottleTest,
    NavigationForPendingSignedInSupervisedUsersAllowsYouTubeInfrastructureInSubframes) {
  SupervisedUserTestEnvironment::EnableSupervisedAccount(identity_manager());
  SetInvalidRefreshTokenForPrimaryAccount(
      identity_manager(),
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  // An invalid, signed-in account is not authenticated.
  CoreAccountInfo account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  signin::SetListAccountsResponseOneAccountWithParams(
      {account_info.email, account_info.gaia,
       /*valid=*/false,
       /*is_signed_out=*/false,
       /*verified=*/true},
      GetTestURLLoaderFactory());
  identity_manager()->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  content::RunAllTasksUntilIdle();

  // Regular youtube content is not allowed, neither in subframe nor in main
  // frame.
  EXPECT_EQ(
      content::NavigationThrottle::CANCEL,
      CreateNavigationThrottle(GURL(kYoutubeDomain), /*skip_jni_call=*/true,
                               /*for_subframe=*/true)
          ->throttles()
          .back()
          ->WillStartRequest());
  EXPECT_EQ(
      content::NavigationThrottle::CANCEL,
      CreateNavigationThrottle(GURL(kYoutubeDomain), /*skip_jni_call=*/true,
                               /*for_subframe=*/false)
          ->throttles()
          .back()
          ->WillStartRequest());

  // But youtube accounts infrastructure is allowed (only in subframes).
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kYoutubeAccountsDomain),
                                     /*skip_jni_call=*/true,
                                     /*for_subframe=*/true)
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            CreateNavigationThrottle(GURL(kYoutubeAccountsDomain),
                                     /*skip_jni_call=*/true,
                                     /*for_subframe=*/false)
                ->throttles()
                .back()
                ->WillStartRequest());
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest,
       NavigationForNotFreshSupervisedUsers) {
  SupervisedUserTestEnvironment::EnableSupervisedAccount(identity_manager());
  signin::SetFreshnessOfAccountsInGaiaCookie(identity_manager(), false);

  // For supervised users that are stale, navigation to Google and
  //  Youtube are deferred.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            CreateNavigationThrottle(GURL(kExampleURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleSearchURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kGoogleHomeURL))
                ->throttles()
                .back()
                ->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            CreateNavigationThrottle(GURL(kYoutubeDomain))
                ->throttles()
                .back()
                ->WillStartRequest());

  // Prerendering is not supported for supervised users.
  std::unique_ptr<content::MockNavigationThrottleRegistry> registry =
      CreateNavigationThrottle(GURL(kExampleURL));
  raw_ptr<SupervisedUserGoogleAuthNavigationThrottle> prerendered_throttle =
      static_cast<SupervisedUserGoogleAuthNavigationThrottle*>(
          registry->throttles().back().get());

  EXPECT_CALL(*static_cast<content::MockNavigationHandle*>(
                  prerendered_throttle->navigation_handle()),
              IsInPrerenderedMainFrame())
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            prerendered_throttle->WillStartRequest());
}

TEST_F(SupervisedUserGoogleAuthNavigationThrottleTest, NavigationForNonUsers) {
  EXPECT_EQ(
      0u,
      CreateNavigationThrottle(GURL(kExampleURL), false)->throttles().size());
}

}  // namespace
}  // namespace supervised_user
