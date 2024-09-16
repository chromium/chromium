// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_core_host.h"

#include <memory>
#include <optional>

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
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/ip_protection/common/ip_protection_config_http.h"
#include "components/ip_protection/common/ip_protection_core_host_helper.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/mock_blind_sign_auth.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

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
                   .city_name = "ALABASTER"}) {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false, /*should_record_metrics=*/false);
    tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            prefs(),
            /*host_content_settings_map=*/host_content_settings_map_.get(),
            /*is_incognito=*/false);
    auto bsa = std::make_unique<ip_protection::MockBlindSignAuth>();
    bsa_ = bsa.get();
    core_host_ = std::make_unique<IpProtectionCoreHost>(
        IdentityManager(), tracking_protection_settings_.get(), prefs(),
        /*profile=*/nullptr);
    core_host_->SetUpForTesting(test_url_loader_factory_.GetSafeWeakWrapper(),
                                std::move(bsa));

    token_server_get_proxy_config_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}));
    ASSERT_TRUE(token_server_get_proxy_config_url_.is_valid());
  }

  void TearDown() override {
    // Remove the raw_ptr to the Mock BSA before `core_host_` frees it.
    bsa_ = nullptr;
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
    core_host_->Shutdown();
  }

  // Get the IdentityManager for this test.
  signin::IdentityManager* IdentityManager() {
    return identity_test_env_.identity_manager();
  }

  void SetupAccount() {
    if (primary_account_behavior_ == PrimaryAccountBehavior::kNone) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
                "access_token", base::Time::Now());
        break;
    }
  }

  // Call `TryGetAuthTokens()` and run until it completes.
  void TryGetAuthTokens(int num_tokens,
                        network::mojom::IpProtectionProxyLayer proxy_layer) {
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

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  std::unique_ptr<IpProtectionCoreHost> core_host_;
  // quiche::BlindSignAuthInterface owned and used by the sequence bound
  // ip_protection_token_fetcher_ in core_host_.
  raw_ptr<ip_protection::MockBlindSignAuth> bsa_;
};

// The success case: a primary account is available, and BSA gets a token for
// it.
TEST_F(IpProtectionCoreHostTest, Success) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_tokens({ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_),
                    ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-2", expiration_time_, geo_hint_)});

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_)
                             .value());
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
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

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
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

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
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
      {ip_protection::IpProtectionCoreHostHelper::
           CreateBlindSignTokenForTesting("single-use-1", expiration_time_,
                                          geo_hint_country),
       ip_protection::IpProtectionCoreHostHelper::
           CreateBlindSignTokenForTesting("single-use-2", expiration_time_,
                                          geo_hint_country)});

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_country)
                             .value());
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
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
  bsa_->set_tokens({ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint)});

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
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

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kBugBackoff);
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

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kBugBackoff);
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

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kNotEligibleBackoff);
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

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
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
  bsa_->set_tokens({ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_),
                    ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-2", expiration_time_, geo_hint_)});

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_)
                             .value());
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
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

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedOAuthTokenTransient, 1);
}

// Fetching OAuth token returns a persistent error.
TEST_F(IpProtectionCoreHostTest, AuthTokenPersistentError) {
  primary_account_behavior_ =
      PrimaryAccountBehavior::kTokenFetchPersistentError;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedOAuthTokenPersistent, 1);
}

// No primary account.
TEST_F(IpProtectionCoreHostTest, NoPrimary) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensResult::kFailedNoAccount, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// TryGetAuthTokens() fails because IP Protection is disabled by user settings.
TEST_F(IpProtectionCoreHostTest, TryGetAuthTokens_IpProtectionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(privacy_sandbox::kIpProtectionV1);

  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  prefs()->SetBoolean(prefs::kIpProtectionEnabled, false);

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(IpProtectionCoreHostTest, AccountLoginTriggersBackoffReset) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());

  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->set_tokens({ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_)});

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

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
  core_host_->TryGetAuthTokens(1,
                               network::mojom::IpProtectionProxyLayer::kProxyB,
                               tokens_future.GetCallback());
  const std::optional<base::Time>& try_again_after =
      tokens_future.Get<std::optional<base::Time>>();
  ASSERT_TRUE(try_again_after);
  EXPECT_EQ(*try_again_after, base::Time::Max());

  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::State::NONE));

  bsa_->set_tokens({ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_)});
  tokens_future.Clear();
  core_host_->TryGetAuthTokens(1,
                               network::mojom::IpProtectionProxyLayer::kProxyB,
                               tokens_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now());
  const std::optional<std::vector<BlindSignedAuthToken>>& tokens =
      tokens_future.Get<std::optional<std::vector<BlindSignedAuthToken>>>();
  ASSERT_TRUE(tokens);
}

// Backoff calculations.
TEST_F(IpProtectionCoreHostTest, CalculateBackoff) {
  using enum ip_protection::TryGetAuthTokensResult;

  auto check = [&](ip_protection::TryGetAuthTokensResult result,
                   std::optional<base::TimeDelta> backoff, bool exponential) {
    SCOPED_TRACE(::testing::Message()
                 << "result: " << static_cast<int>(result));
    EXPECT_EQ(core_host_->CalculateBackoff(result), backoff);
    if (backoff && exponential) {
      EXPECT_EQ(core_host_->CalculateBackoff(result), (*backoff) * 2);
      EXPECT_EQ(core_host_->CalculateBackoff(result), (*backoff) * 4);
    } else {
      EXPECT_EQ(core_host_->CalculateBackoff(result), backoff);
    }
  };

  check(kSuccess, std::nullopt, false);
  check(kFailedNotEligible,
        ip_protection::IpProtectionCoreHostHelper::kNotEligibleBackoff, false);
  check(kFailedBSA400, ip_protection::IpProtectionCoreHostHelper::kBugBackoff,
        true);
  check(kFailedBSA401, ip_protection::IpProtectionCoreHostHelper::kBugBackoff,
        true);
  check(kFailedBSA403,
        ip_protection::IpProtectionCoreHostHelper::kNotEligibleBackoff, false);
  check(kFailedBSAOther,
        ip_protection::IpProtectionCoreHostHelper::kTransientBackoff, true);
  check(kFailedOAuthTokenTransient,
        ip_protection::IpProtectionCoreHostHelper::kTransientBackoff, true);

  check(kFailedNoAccount, base::TimeDelta::Max(), false);
  // The account-related backoffs should not be changed except by account change
  // events.
  check(kFailedBSA400, base::TimeDelta::Max(), false);
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  // The backoff time should have been reset.
  check(kFailedBSA400, ip_protection::IpProtectionCoreHostHelper::kBugBackoff,
        true);

  check(kFailedOAuthTokenPersistent, base::TimeDelta::Max(), false);
  check(kFailedBSA400, base::TimeDelta::Max(), false);
  // Change the refresh token error state to an error state and then back to a
  // no-error state so that the latter clears the backoff time.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::State::NONE));
  check(kFailedBSA400, ip_protection::IpProtectionCoreHostHelper::kBugBackoff,
        true);
}

TEST_F(IpProtectionCoreHostTest, GetProxyListWithApiKey) {
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

  core_host_->GetProxyList(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(),
              testing::ElementsAreArray(response_proxy_list));

  ASSERT_TRUE(geo_hint.has_value());  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == geo_hint_);
}

TEST_F(IpProtectionCoreHostTest, GetProxyListWithOAuthToken) {
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
  core_host_->GetProxyList(proxy_list_future_.GetCallback());
  RespondToAccessTokenRequest();
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

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

  core_host_->GetProxyList(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(),
              testing::ElementsAreArray(proxy_override_list));

  ASSERT_TRUE(geo_hint.has_value());  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == geo_hint_);
}

TEST_F(IpProtectionCoreHostTest, GetProxyListFailure) {
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
    this->core_host_->GetProxyList(future.GetCallback());
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

  // An immediate second call to GetProxyList should not call the retriever
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

  // An immediate second call to GetProxyList is also allowed to proceed.
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
}

TEST_F(IpProtectionCoreHostTest, GetProxyList_IpProtectionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(privacy_sandbox::kIpProtectionV1);

  prefs()->SetBoolean(prefs::kIpProtectionEnabled, false);

  core_host_->GetProxyList(proxy_list_future_.GetCallback());

  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_FALSE(geo_hint.has_value());
}

// Do a basic check of the token formats.
TEST_F(IpProtectionCoreHostTest, TokenFormat) {
  BlindSignedAuthToken result =
      ip_protection::IpProtectionCoreHostHelper::
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
