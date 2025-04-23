// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_core_host.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/ip_protection/common/ip_protection_config_http.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/mock_blind_sign_auth.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif

using ::ip_protection::BlindSignedAuthToken;
using ::ip_protection::GeoHint;

namespace {

constexpr char kTryGetAuthTokensResultHistogram[] =
    "NetworkService.IpProtection.TryGetAuthTokensResult";
constexpr char kOAuthTokenFetchHistogram[] =
    "NetworkService.IpProtection.OAuthTokenFetchTime";
constexpr char kTryGetAuthTokensErrorHistogram[] =
    "NetworkService.IpProtection.TryGetAuthTokensErrors";
constexpr char kTokenBatchHistogram[] =
    "NetworkService.IpProtection.TokenBatchRequestTime";

constexpr char kTestEmail[] = "test@example.com";
constexpr size_t kPRTPointSize = 33;

enum class PrimaryAccountBehavior {
  // Primary account not set.
  kNone,

  // Primary account exists but returns a transient error fetching access token.
  kTokenFetchTransientError,

  // Primary account exists but returns a persistent error fetching access
  // token.
  kTokenFetchPersistentError,

  // Primary account exists but is not eligible for IP protection.
  kIneligible,

  // Like kIneligible, but also a dogfooder.
  kIneligibleDogfooder,

  // Primary account exists but eligibility is kUnknown.
  kUnknownEligibility,

  // Primary account exists, is eligible, and returns OAuth token
  // "access_token".
  kReturnsToken,
};

}  // namespace

class IpProtectionCoreHostTest : public testing::Test {
 protected:
  IpProtectionCoreHostTest()
      : expiration_time_(base::Time::Now() + base::Hours(1)),
        geo_hint_({.country_code = "US",
                   .iso_region = "US-AL",
                   .city_name = "ALABASTER"}),
        enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    variations::TestVariationsService::RegisterPrefs(prefs()->registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        prefs(), &enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        prefs(), metrics_state_manager_.get());
  }

  void SetUp() override {
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false, /*should_record_metrics=*/false);
    auto bsa = std::make_unique<ip_protection::MockBlindSignAuth>();
    bsa_ = bsa.get();
#if BUILDFLAG(IS_CHROMEOS)
    install_attributes_ = std::make_unique<ash::StubInstallAttributes>();
    ash::InstallAttributes::SetForTesting(install_attributes_.get());
#endif
    management_service_ = std::make_unique<policy::ManagementService>(
        std::vector<std::unique_ptr<policy::ManagementStatusProvider>>());
    tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            prefs(),
            /*host_content_settings_map=*/host_content_settings_map_.get(),
            /*management_service=*/management_service_.get(),
            /*is_incognito=*/false);
    core_host_ = std::make_unique<IpProtectionCoreHost>(
        IdentityManager(), tracking_protection_settings_.get(), prefs(),
        /*profile=*/nullptr);
    core_host_->SetUpForTesting(test_url_loader_factory_.GetSafeWeakWrapper(),
                                std::move(bsa));

    token_server_get_proxy_config_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}));
    ASSERT_TRUE(token_server_get_proxy_config_url_.is_valid());

    default_not_eligible_backoff_ =
        net::features::kIpPrivacyTryGetAuthTokensNotEligibleBackoff.Get();
    default_transient_backoff_ =
        net::features::kIpPrivacyTryGetAuthTokensTransientBackoff.Get();
    default_bug_backoff_ =
        net::features::kIpPrivacyTryGetAuthTokensBugBackoff.Get();

    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);

    // Remove the raw_ptr to the Mock BSA before `core_host_` frees it.
    bsa_ = nullptr;
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
    core_host_->Shutdown();
#if BUILDFLAG(IS_CHROMEOS)
    ash::InstallAttributes::ShutdownForTesting();
#endif
  }

  // Get the IdentityManager for this test.
  signin::IdentityManager* IdentityManager() {
    return identity_test_env_.identity_manager();
  }

  void SetupAccount() {
    if (primary_account_behavior_ == PrimaryAccountBehavior::kNone) {
#if !BUILDFLAG(IS_CHROMEOS)
      // Simulate a log out event on all platforms except ChromeOS Ash where
      // this is not supported.
      identity_test_env_.ClearPrimaryAccount();
#endif
    } else {
      identity_test_env_.MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

      if (primary_account_behavior_ == PrimaryAccountBehavior::kIneligible) {
        SetCanUseChromeIpProtectionCapability(false);
      } else {
        SetCanUseChromeIpProtectionCapability(true);
      }

      variations_service_->SetIsLikelyDogfoodClientForTesting(
          primary_account_behavior_ ==
          PrimaryAccountBehavior::kIneligibleDogfooder);
    }
  }

  void RespondToAccessTokenRequest() {
    switch (primary_account_behavior_) {
      case PrimaryAccountBehavior::kNone:
      case PrimaryAccountBehavior::kIneligible:
        break;
      case PrimaryAccountBehavior::kTokenFetchTransientError:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
                GoogleServiceAuthError(
                    GoogleServiceAuthError::CONNECTION_FAILED));
        break;
      case PrimaryAccountBehavior::kTokenFetchPersistentError:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
                GoogleServiceAuthError(
                    GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
        break;
      case PrimaryAccountBehavior::kUnknownEligibility:
      case PrimaryAccountBehavior::kReturnsToken:
      case PrimaryAccountBehavior::kIneligibleDogfooder:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
                "access_token", base::Time::Now());
        break;
    }
  }

  // Call `TryGetAuthTokens()` and run until it completes.
  void TryGetAuthTokens(int num_tokens, ip_protection::ProxyLayer proxy_layer) {
    SetupAccount();

    core_host_->TryGetAuthTokens(num_tokens, proxy_layer,
                                 tokens_future_.GetCallback());

    RespondToAccessTokenRequest();
    ASSERT_TRUE(tokens_future_.Wait()) << "TryGetAuthTokens did not call back";
  }

  // Set the CanUseChromeIpProtection account capability. The capability tribool
  // defaults to `kUnknown`.
  void SetCanUseChromeIpProtectionCapability(bool enabled) {
    auto account_info = identity_test_env_.identity_manager()
                            ->FindExtendedAccountInfoByEmailAddress(kTestEmail);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_chrome_ip_protection(enabled);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
  }

  // Expect that the TryGetAuthTokens call returned the given tokens.
  void ExpectTryGetAuthTokensResult(
      std::vector<BlindSignedAuthToken> bsa_tokens) {
    EXPECT_EQ(std::get<0>(tokens_future_.Get()), bsa_tokens);
  }

  // Expect that the TryGetAuthTokens call returned nullopt, with
  // `try_again_after` at the given delta from the current time.
  void ExpectTryGetAuthTokensResultFailed(base::TimeDelta try_again_delta) {
    auto& [bsa_tokens, try_again_after] = tokens_future_.Get();
    EXPECT_EQ(bsa_tokens, std::nullopt);
    if (!bsa_tokens) {
      EXPECT_EQ(*try_again_after, base::Time::Now() + try_again_delta);
    }
    // Clear future so it can be reused and accept new tokens.
    tokens_future_.Clear();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  // The behavior of the identity manager.
  PrimaryAccountBehavior primary_account_behavior_ =
      PrimaryAccountBehavior::kReturnsToken;

  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::TestFuture<
      const std::optional<std::vector<BlindSignedAuthToken>>&,
      std::optional<base::Time>>
      tokens_future_;

  base::test::TestFuture<const std::optional<std::vector<net::ProxyChain>>&,
                         const std::optional<GeoHint>&>
      proxy_list_future_;

  // URL loader factory used for all fetchers.
  network::TestURLLoaderFactory test_url_loader_factory_;

  // URL at which getProxyConfig is invoked.
  GURL token_server_get_proxy_config_url_;

  // Test environment for IdentityManager. This must come after the
  // TaskEnvironment.
  signin::IdentityTestEnvironment identity_test_env_;

  // A convenient expiration time for fake tokens, in the future.
  base::Time expiration_time_;

  // A convenient geo hint for fake tokens.
  GeoHint geo_hint_;

  base::HistogramTester histogram_tester_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::StubInstallAttributes> install_attributes_;
#endif
  std::unique_ptr<policy::ManagementService> management_service_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  std::unique_ptr<IpProtectionCoreHost> core_host_;
  // quiche::BlindSignAuthInterface owned and used by the sequence bound
  // ip_protection_token_fetcher_ in core_host_.
  raw_ptr<ip_protection::MockBlindSignAuth> bsa_;

  // Variations service and associated values, used to indicate
  // whether the client is dogfooding.
  metrics::TestEnabledStateProvider enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;

  // Default backoff times applied for calculating `try_again_after`.
  base::TimeDelta default_not_eligible_backoff_;
  base::TimeDelta default_transient_backoff_;
  base::TimeDelta default_bug_backoff_;
};

// NOTE: Many of these tests are similar those for
// IpProtectionTokenDirectFetcher, but both make sense. In the fetcher, they
// serve as unit tests with a fake delegate. Here, they incorporate
// IpProtectionCoreHost as a delegate.

// The success case: a primary account is available, and BSA gets a token for
// it.
TEST_F(IpProtectionCoreHostTest, Success) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_tokens({ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_),
                    ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-2", expiration_time_, geo_hint_)});

  TryGetAuthTokens(2, ip_protection::ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionTokenFetcherHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_)
                             .value());
  expected.push_back(ip_protection::IpProtectionTokenFetcherHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-2", expiration_time_, geo_hint_)
                             .value());
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(IpProtectionCoreHostTest, NoTokens) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns malformed tokens.
TEST_F(IpProtectionCoreHostTest, MalformedTokens) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  auto geo_hint = anonymous_tokens::GeoHint{
      .geo_hint = "US,US-CA,MOUNTAIN VIEW",
      .country_code = "US",
      .region = "US-CA",
      .city = "MOUNTAIN VIEW",
  };
  bsa_->set_tokens(
      {{"invalid-token-proto-data", absl::Now() + absl::Hours(1), geo_hint}});

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

TEST_F(IpProtectionCoreHostTest, TokenGeoHintContainsOnlyCountry) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  GeoHint geo_hint_country;
  geo_hint_country.country_code = "US";
  bsa_->set_tokens(
      {ip_protection::IpProtectionTokenFetcherHelper::
           CreateBlindSignTokenForTesting("single-use-1", expiration_time_,
                                          geo_hint_country),
       ip_protection::IpProtectionTokenFetcherHelper::
           CreateBlindSignTokenForTesting("single-use-2", expiration_time_,
                                          geo_hint_country)});

  TryGetAuthTokens(2, ip_protection::ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionTokenFetcherHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_country)
                             .value());
  expected.push_back(ip_protection::IpProtectionTokenFetcherHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-2", expiration_time_, geo_hint_country)
                             .value());
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

TEST_F(IpProtectionCoreHostTest, TokenHasMissingGeoHint) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  GeoHint geo_hint;
  bsa_->set_tokens({ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint)});

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 400 error.
TEST_F(IpProtectionCoreHostTest, BlindSignedTokenError400) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_status(absl::InvalidArgumentError("uhoh"));

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_bug_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedBSA400, 1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      4043967578,  // base::PersistentHash("INVALID_ARGUMENT: uhoh")
      1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 401 error.
TEST_F(IpProtectionCoreHostTest, BlindSignedTokenError401) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_status(absl::UnauthenticatedError("uhoh"));

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_bug_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedBSA401, 1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      4264091263,  // base::PersistentHash("UNAUTHENTICATED: uhoh")
      1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 403 error.
TEST_F(IpProtectionCoreHostTest, BlindSignedTokenError403) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_status(absl::PermissionDeniedError("uhoh"));

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_not_eligible_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedBSA403, 1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      4104528123,  // base::PersistentHash("PERMISSION_DENIED: uhoh")
      1);
  // Failed to parse GetInitialDataResponse
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns some other error.
TEST_F(IpProtectionCoreHostTest, BlindSignedTokenErrorOther) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_status(absl::UnknownError("uhoh"));

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      2844845398,  // base::PersistentHash("UNKNOWN: uhoh")
      1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// The CanUseChromeIpProtection capability is not present (`kUnknown`).
TEST_F(IpProtectionCoreHostTest, AccountCapabilityUnknown) {
  primary_account_behavior_ = PrimaryAccountBehavior::kUnknownEligibility;
  bsa_->set_tokens({ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_),
                    ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-2", expiration_time_, geo_hint_)});

  TryGetAuthTokens(2, ip_protection::ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionTokenFetcherHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_)
                             .value());
  expected.push_back(ip_protection::IpProtectionTokenFetcherHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-2", expiration_time_, geo_hint_)
                             .value());
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// Fetching OAuth token returns a transient error.
TEST_F(IpProtectionCoreHostTest, AuthTokenTransientError) {
  primary_account_behavior_ = PrimaryAccountBehavior::kTokenFetchTransientError;

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedOAuthTokenTransient, 1);
}

// Fetching OAuth token returns a persistent error.
TEST_F(IpProtectionCoreHostTest, AuthTokenPersistentError) {
  primary_account_behavior_ =
      PrimaryAccountBehavior::kTokenFetchPersistentError;

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedOAuthTokenPersistent, 1);
}

// No primary account.
TEST_F(IpProtectionCoreHostTest, NoPrimary) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedNoAccount, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// Primary account exists but does not have CanUseChromeIpProtection.
TEST_F(IpProtectionCoreHostTest, NoCapability) {
  primary_account_behavior_ = PrimaryAccountBehavior::kIneligible;

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(default_not_eligible_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedNotEligible, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// Primary account exists, does not have CanUseChromeIpProtection, but is a
// dogfooder.
TEST_F(IpProtectionCoreHostTest, NoCapabilityButDogfooder) {
  primary_account_behavior_ = PrimaryAccountBehavior::kIneligibleDogfooder;
  bsa_->set_tokens({ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_)});

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionTokenFetcherHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_)
                             .value());
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// TryGetAuthTokens() fails because IP Protection is disabled by user settings.
TEST_F(IpProtectionCoreHostTest, TryGetAuthTokens_IpProtectionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(privacy_sandbox::kIpProtectionUx);

  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  prefs()->SetBoolean(prefs::kIpProtectionEnabled, false);

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedDisabledByUser, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// No primary account initially but this changes when the account status
// changes.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(IpProtectionCoreHostTest, AccountLoginTriggersBackoffReset) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());

  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_tokens({ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_)});

  TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
}
#endif

// If the account session token expires and is renewed, the persistent backoff
// should be cleared.
TEST_F(IpProtectionCoreHostTest, SessionRefreshTriggersBackoffReset) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  SetCanUseChromeIpProtectionCapability(true);

  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  base::test::TestFuture<
      const std::optional<std::vector<BlindSignedAuthToken>>&,
      std::optional<base::Time>>
      tokens_future;
  core_host_->TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB,
                               tokens_future.GetCallback());
  const std::optional<base::Time>& try_again_after =
      tokens_future.Get<std::optional<base::Time>>();
  ASSERT_TRUE(try_again_after);
  EXPECT_EQ(*try_again_after, base::Time::Max());

  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::State::NONE));

  bsa_->set_tokens({ip_protection::IpProtectionTokenFetcherHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_)});
  tokens_future.Clear();
  core_host_->TryGetAuthTokens(1, ip_protection::ProxyLayer::kProxyB,
                               tokens_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now());
  const std::optional<std::vector<BlindSignedAuthToken>>& tokens =
      tokens_future.Get<std::optional<std::vector<BlindSignedAuthToken>>>();
  ASSERT_TRUE(tokens);
}

TEST_F(IpProtectionCoreHostTest, GetProxyConfigWithApiKey) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {
          {net::features::kIpPrivacyIncludeOAuthTokenInGetProxyConfig.name,
           "false"},
      });
  std::vector<net::ProxyChain> response_proxy_list = {
      ip_protection::IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxyA", "proxyB"}),
  };

  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxyA");
  chain->set_proxy_b("proxyB");
  response.mutable_geo_hint()->set_country_code(geo_hint_.country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_.iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_.city_name);
  std::string response_str = response.SerializeAsString();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_proxy_config_url_);
        EXPECT_TRUE(request.headers.HasHeader("X-Goog-Api-Key"));
        EXPECT_FALSE(request.headers.HasHeader("Authentication"));
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            token_server_get_proxy_config_url_, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  core_host_->GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(),
              testing::ElementsAreArray(response_proxy_list));

  ASSERT_TRUE(geo_hint.has_value());  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == geo_hint_);
}

TEST_F(IpProtectionCoreHostTest, GetProxyConfigWithOAuthToken) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {
          {net::features::kIpPrivacyIncludeOAuthTokenInGetProxyConfig.name,
           "true"},
      });
  std::vector<net::ProxyChain> response_proxy_list = {
      ip_protection::IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxyA", "proxyB"}),
  };

  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxyA");
  chain->set_proxy_b("proxyB");
  response.mutable_geo_hint()->set_country_code(geo_hint_.country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_.iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_.city_name);
  std::string response_str = response.SerializeAsString();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_proxy_config_url_);
        EXPECT_FALSE(request.headers.HasHeader("X-Goog-Api-Key"));
        EXPECT_TRUE(request.headers.HasHeader("Authorization"));
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            token_server_get_proxy_config_url_, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  SetupAccount();
  core_host_->GetProxyConfig(proxy_list_future_.GetCallback());
  RespondToAccessTokenRequest();
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(),
              testing::ElementsAreArray(response_proxy_list));

  ASSERT_TRUE(geo_hint.has_value());  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == geo_hint_);
}

TEST_F(IpProtectionCoreHostTest, ProxyOverrideFlagsAll) {
  std::vector<net::ProxyChain> proxy_override_list = {
      ip_protection::IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxyAOverride", "proxyBOverride"}),
      ip_protection::IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxyAOverride", "proxyBOverride"}),
  };
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {
          {net::features::kIpPrivacyProxyAHostnameOverride.name,
           "proxyAOverride"},
          {net::features::kIpPrivacyProxyBHostnameOverride.name,
           "proxyBOverride"},
      });
  std::vector<std::vector<std::string>> response_proxy_list = {
      {"proxyA1", "proxyB1"}, {"proxyA2", "proxyB2"}};
  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxyA1");
  chain->set_proxy_b("proxyB1");
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxyA2");
  chain->set_proxy_b("proxyB2");

  response.mutable_geo_hint()->set_country_code(geo_hint_.country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_.iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_.city_name);
  std::string response_str = response.SerializeAsString();

  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_.spec(), response_str);

  core_host_->GetProxyConfig(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(),
              testing::ElementsAreArray(proxy_override_list));

  ASSERT_TRUE(geo_hint.has_value());  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == geo_hint_);
}

TEST_F(IpProtectionCoreHostTest, GetProxyConfigFailure) {
  // Count each call to the retriever's GetProxyConfig and return an error.
  int get_proxy_config_calls = 0;
  bool get_proxy_config_fails = true;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        ASSERT_EQ(request.url, token_server_get_proxy_config_url_);
        get_proxy_config_calls++;
        if (get_proxy_config_fails) {
          test_url_loader_factory_.AddResponse(
              token_server_get_proxy_config_url_.spec(), "",
              net::HTTP_INTERNAL_SERVER_ERROR);
        } else {
          ip_protection::GetProxyConfigResponse response;
          test_url_loader_factory_.AddResponse(
              token_server_get_proxy_config_url_.spec(),
              response.SerializeAsString());
        }
      }));

  auto call_get_proxy_list = [this](bool expect_success) {
    base::test::TestFuture<const std::optional<std::vector<net::ProxyChain>>&,
                           const std::optional<GeoHint>&>
        future;
    this->core_host_->GetProxyConfig(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    // Extract tuple elements for individual comparison.
    const auto& [proxy_list, geo_hint] = future.Get();
    if (expect_success) {
      ASSERT_TRUE(
          proxy_list.has_value());  // Check if optional vector has value.
      EXPECT_EQ(proxy_list,
                std::make_optional<std::vector<net::ProxyChain>>({}));

      // `GetProxyConfigResponse` used is default instance which means GeoHint
      // will not be populated and should be a nullptr.
      EXPECT_FALSE(geo_hint.has_value());
    } else {
      EXPECT_EQ(proxy_list, std::nullopt);
      EXPECT_FALSE(geo_hint.has_value());
    }
  };

  call_get_proxy_list(/*expect_success=*/false);
  EXPECT_EQ(get_proxy_config_calls, 1);

  // An immediate second call to GetProxyConfig should not call the retriever
  // again.
  call_get_proxy_list(/*expect_success=*/false);
  EXPECT_EQ(get_proxy_config_calls, 1);

  const base::TimeDelta timeout = ip_protection::
      IpProtectionProxyConfigDirectFetcher::kGetProxyConfigFailureTimeout;

  // A call after the timeout is allowed to proceed, but fails so the new
  // backoff is 2*timeout.
  task_environment_.FastForwardBy(timeout);
  call_get_proxy_list(/*expect_success=*/false);
  EXPECT_EQ(get_proxy_config_calls, 2);

  // A call another timeout later does nothing because the backoff is 2*timeout
  // now.
  task_environment_.FastForwardBy(timeout);
  call_get_proxy_list(/*expect_success=*/false);
  EXPECT_EQ(get_proxy_config_calls, 2);

  // A call another timeout later is allowed to proceed, and this time succeeds.
  get_proxy_config_fails = false;
  task_environment_.FastForwardBy(timeout);
  call_get_proxy_list(/*expect_success=*/true);
  EXPECT_EQ(get_proxy_config_calls, 3);

  // An immediate second call to GetProxyConfig is also allowed to proceed.
  // Note that the network service also applies a minimum time between calls,
  // so this would not happen in production.
  get_proxy_config_fails = true;
  call_get_proxy_list(/*expect_success=*/false);
  EXPECT_EQ(get_proxy_config_calls, 4);

  // The backoff timeout starts over.
  call_get_proxy_list(/*expect_success=*/false);
  EXPECT_EQ(get_proxy_config_calls, 4);
  task_environment_.FastForwardBy(timeout);
  call_get_proxy_list(/*expect_success=*/false);
  EXPECT_EQ(get_proxy_config_calls, 5);

  get_proxy_config_fails = false;
  // SetupAccount should trigger ClearOAuthTokenProblemBackoff() and reset
  // fetcher's backoff.
  SetupAccount();
  call_get_proxy_list(/*expect_success=*/true);
  EXPECT_EQ(get_proxy_config_calls, 6);
}

TEST_F(IpProtectionCoreHostTest, GetProxyConfig_IpProtectionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(privacy_sandbox::kIpProtectionUx);

  prefs()->SetBoolean(prefs::kIpProtectionEnabled, false);

  core_host_->GetProxyConfig(proxy_list_future_.GetCallback());

  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyConfig did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_FALSE(geo_hint.has_value());
}

TEST_F(IpProtectionCoreHostTest, TryGetProbabilisticRevealTokensSuccess) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);
  const std::uint64_t expiration =
      (base::Time::Now() + base::Hours(10)).InSecondsFSinceUnixEpoch();
  const std::uint64_t next_start =
      (base::Time::Now() + base::Hours(5)).InSecondsFSinceUnixEpoch();
  const int32_t num_tokens_with_signal = 3;
  std::string public_key;
  ASSERT_TRUE(base::Base64Decode("Aibvzr0O6eNKZpGH4Ys6kSKy9zOUW2Scyfn5Ien52tgS",
                                 &public_key));
  std::string response_str;
  {
    ip_protection::GetProbabilisticRevealTokenResponse response_proto;
    for (size_t i = 0; i < 10; ++i) {
      ip_protection::
          GetProbabilisticRevealTokenResponse_ProbabilisticRevealToken* token =
              response_proto.add_tokens();
      token->set_version(1);
      token->set_u(std::string(kPRTPointSize, 'u'));
      token->set_e(std::string(kPRTPointSize, 'e'));
    }
    response_proto.mutable_public_key()->set_y(public_key);
    response_proto.mutable_expiration_time()->set_seconds(expiration);
    response_proto.mutable_next_epoch_start_time()->set_seconds(next_start);
    response_proto.set_num_tokens_with_signal(num_tokens_with_signal);
    response_proto.set_epoch_id(std::string(8, '0'));
    response_str = response_proto.SerializeAsString();
  }

  const GURL issuer_server_url =
      GURL("https://aaftokenissuer.pa.googleapis.com/v1/issueprts");
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            issuer_server_url, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<
      const std::optional<
          ip_protection::TryGetProbabilisticRevealTokensOutcome>&,
      const ip_protection::TryGetProbabilisticRevealTokensResult&>
      future;

  core_host_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait())
      << "TryGetProbabilisticRevealTokens did not call back";

  const auto& outcome = future.Get<0>();
  ASSERT_TRUE(outcome);
  EXPECT_THAT(outcome->tokens, testing::SizeIs(10));
  EXPECT_EQ(outcome->tokens[9].u, std::string(kPRTPointSize, 'u'));
  EXPECT_EQ(outcome->tokens[9].e, std::string(kPRTPointSize, 'e'));
  EXPECT_EQ(outcome->public_key, public_key);
  EXPECT_EQ(outcome->expiration_time_seconds, expiration);
  EXPECT_EQ(outcome->next_epoch_start_time_seconds, next_start);
  EXPECT_EQ(outcome->num_tokens_with_signal, num_tokens_with_signal);

  const auto& result = std::move(future.Get<1>());
  EXPECT_EQ(result.status,
            ip_protection::TryGetProbabilisticRevealTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST_F(IpProtectionCoreHostTest,
       TryGetProbabilisticRevealTokensInvalidPublicKey) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);
  const std::uint64_t expiration =
      (base::Time::Now() + base::Hours(9)).InSecondsFSinceUnixEpoch();
  const std::uint64_t next_start =
      (base::Time::Now() + base::Hours(4)).InSecondsFSinceUnixEpoch();
  const int32_t num_tokens_with_signal = 7;
  const std::string public_key = "invalid-public-key";
  std::string response_str;
  {
    ip_protection::GetProbabilisticRevealTokenResponse response_proto;
    for (size_t i = 0; i < 13; ++i) {
      ip_protection::
          GetProbabilisticRevealTokenResponse_ProbabilisticRevealToken* token =
              response_proto.add_tokens();
      token->set_version(1);
      token->set_u(std::string(29, 'u'));
      token->set_e(std::string(29, 'e'));
    }
    response_proto.mutable_public_key()->set_y(public_key);
    response_proto.mutable_expiration_time()->set_seconds(expiration);
    response_proto.mutable_next_epoch_start_time()->set_seconds(next_start);
    response_proto.set_num_tokens_with_signal(num_tokens_with_signal);
    response_str = response_proto.SerializeAsString();
  }

  const GURL issuer_server_url =
      GURL("https://aaftokenissuer.pa.googleapis.com/v1/issueprts");
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        auto head = network::mojom::URLResponseHead::New();
        test_url_loader_factory_.AddResponse(
            issuer_server_url, std::move(head), response_str,
            network::URLLoaderCompletionStatus(net::OK));
      }));

  base::test::TestFuture<
      const std::optional<
          ip_protection::TryGetProbabilisticRevealTokensOutcome>&,
      const ip_protection::TryGetProbabilisticRevealTokensResult&>
      future;

  core_host_->TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait())
      << "TryGetProbabilisticRevealTokens did not call back";

  const auto& outcome = future.Get<0>();
  EXPECT_FALSE(outcome);

  const auto& result = std::move(future.Get<1>());
  EXPECT_EQ(
      result.status,
      ip_protection::TryGetProbabilisticRevealTokensStatus::kInvalidPublicKey);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

// Do a basic check of the token formats.
TEST_F(IpProtectionCoreHostTest, TokenFormat) {
  BlindSignedAuthToken result =
      ip_protection::IpProtectionTokenFetcherHelper::
          CreateMockBlindSignedAuthTokenForTesting("single-use-1",
                                                   expiration_time_, geo_hint_)
              .value();
  std::string& token = result.token;
  size_t token_position = token.find("PrivateToken token=");
  size_t extensions_position = token.find("extensions=");

  EXPECT_EQ(token_position, 0u);
  EXPECT_NE(extensions_position, std::string::npos);

  // Check if the comma is between "PrivateToken token=" and "extensions=".
  size_t comma_position = token.find(",", token_position);
  EXPECT_NE(comma_position, std::string::npos);
  EXPECT_LT(comma_position, extensions_position);
}
