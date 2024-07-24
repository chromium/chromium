// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/process/process_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/net/cookie_encryption_provider_impl.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "chrome/browser/os_crypt/test_support.h"
#include "chrome/install_static/test/scoped_install_details.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

enum TestConfiguration {
  // Network Service is using Sync os_crypt API.
  kOSCryptSync,
  // Network Service is using Async API, i.e. cookie_encryption_provider is
  // being supplied to the profile network context params. No key providers are
  // used in this test configuration.
  kOSCryptAsync,
#if BUILDFLAG(IS_WIN)
  // The App Bound key provider is being registered with Chrome, but not being
  // used for encryption of new data, but will decrypt any existing data.
  kOSCryptAsyncWithAppBoundProvider,
  // The App Bound key provider is being registered with Chrome, and is being
  // used for encryption of new data.
  kOSCryptAsyncWithAppBoundProviderWithEncryption,
  // This is the same as `kOSCryptAsyncWithAppBoundProviderWithEncryption` but
  // without the service being correctly installed/running. This allows testing
  // of failure conditions.
  kOSCryptAsyncWithAppBoundProviderWithEncryptionNoService,
  // This is the same as `kOSCryptAsyncWithAppBoundProviderWithEncryption` but
  // with custom user data dir meaning that Encrypt should fail but Decrypt
  // should work. This is to test that on a machine where data was previously
  // encrypted, then it moved to an unsupported state, decryption will still be
  // attempted.
  kOSCryptAsyncWithAppBoundProviderWithEncryptionUnsupportedUserData,
  // This is the same as `kOSCryptAsyncWithAppBoundProvider` but with App-Bound
  // encryption disabled by policy. If run on a fresh profile it should not
  // generate or store a key. However, if run on a profile where policy was
  // previously enabled, it should successfully decrypt the key, as there might
  // have been data encrypted with this key before the policy was disabled.
  kOSCryptAsyncWithAppBoundProviderDisabledByPolicy,
#endif  // BUILDFLAG(IS_WIN)
};

enum MetricsExpectation {
  kNotChecked,
  kOSCryptAsyncMetrics,
#if BUILDFLAG(IS_WIN)
  kAppBoundEncryptMetrics,
  kAppBoundDecryptMetrics,
#endif  // BUILDFLAG(IS_WIN)
  kNoMetrics,
};

struct TestCase {
  std::string name;
  bool expect_pass = true;
  TestConfiguration before;
  TestConfiguration after;
  MetricsExpectation metrics_expectation_before = kNotChecked;
  MetricsExpectation metrics_expectation_after = kNotChecked;
};

#if BUILDFLAG(IS_WIN)
bool IsElevationRequired(TestConfiguration configuration) {
  switch (configuration) {
    case kOSCryptSync:
      [[fallthrough]];
    case kOSCryptAsync:
      [[fallthrough]];
    case kOSCryptAsyncWithAppBoundProviderWithEncryptionNoService:
      return false;
    case kOSCryptAsyncWithAppBoundProvider:
      [[fallthrough]];
    case kOSCryptAsyncWithAppBoundProviderWithEncryption:
      [[fallthrough]];
    case kOSCryptAsyncWithAppBoundProviderWithEncryptionUnsupportedUserData:
      [[fallthrough]];
    case kOSCryptAsyncWithAppBoundProviderDisabledByPolicy:
      return true;
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class CookieEncryptionProviderBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestCase> {
 public:
#if BUILDFLAG(IS_WIN)
  CookieEncryptionProviderBrowserTest()
      : scoped_install_details_(
            std::make_unique<os_crypt::FakeInstallDetails>()) {}
#endif  // BUILDFLAG(IS_WIN)

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if ((IsElevationRequired(GetParam().before) ||
         IsElevationRequired(GetParam().after)) &&
        base::GetCurrentProcessIntegrityLevel() != base::HIGH_INTEGRITY) {
      GTEST_SKIP() << "Elevation is required for this test.";
    }
#endif  // BUILDFLAG(IS_WIN)

    auto configuration =
        content::IsPreTest() ? GetParam().before : GetParam().after;

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (configuration) {
      case kOSCryptSync:
        disabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        break;
      case kOSCryptAsync:
        enabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
#if BUILDFLAG(IS_WIN)
        disabled_features.push_back(
            features::kRegisterAppBoundEncryptionProvider);
#endif  // BUILDFLAG(IS_WIN)
        break;
#if BUILDFLAG(IS_WIN)
      case kOSCryptAsyncWithAppBoundProvider:
        maybe_uninstall_service_ = os_crypt::InstallService();
        EXPECT_TRUE(maybe_uninstall_service_.has_value());
        enabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        enabled_features.push_back(
            features::kRegisterAppBoundEncryptionProvider);
        disabled_features.push_back(
            features::kUseAppBoundEncryptionProviderForEncryption);
        break;
      case kOSCryptAsyncWithAppBoundProviderWithEncryption:
        maybe_uninstall_service_ = os_crypt::InstallService();
        EXPECT_TRUE(maybe_uninstall_service_.has_value());
        enabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        enabled_features.push_back(
            features::kRegisterAppBoundEncryptionProvider);
        enabled_features.push_back(
            features::kUseAppBoundEncryptionProviderForEncryption);
        break;
      case kOSCryptAsyncWithAppBoundProviderWithEncryptionNoService:
        enabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        enabled_features.push_back(
            features::kRegisterAppBoundEncryptionProvider);
        enabled_features.push_back(
            features::kUseAppBoundEncryptionProviderForEncryption);
        break;
      case kOSCryptAsyncWithAppBoundProviderWithEncryptionUnsupportedUserData:
        maybe_uninstall_service_ = os_crypt::InstallService();
        EXPECT_TRUE(maybe_uninstall_service_.has_value());
        enabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        enabled_features.push_back(
            features::kRegisterAppBoundEncryptionProvider);
        enabled_features.push_back(
            features::kUseAppBoundEncryptionProviderForEncryption);
        os_crypt::SetNonStandardUserDataDirSupportedForTesting(
            /*supported=*/false);
        break;
      case kOSCryptAsyncWithAppBoundProviderDisabledByPolicy:
        maybe_uninstall_service_ = os_crypt::InstallService();
        EXPECT_TRUE(maybe_uninstall_service_.has_value());
        enabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        enabled_features.push_back(
            features::kRegisterAppBoundEncryptionProvider);
        disabled_features.push_back(
            features::kUseAppBoundEncryptionProviderForEncryption);

        policy_provider_.SetDefaultReturns(
            /*is_initialization_complete_return=*/true,
            /*is_first_policy_load_complete_return=*/true);
        policy::PolicyMap values;
        // Disable App-Bound Encryption by policy.
        values.Set(policy::key::kApplicationBoundEncryptionEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
        policy_provider_.UpdateChromePolicy(values);
        policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
            &policy_provider_);
        break;
#endif  // BUILDFLAG(IS_WIN)
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }

    auto metrics_expectation = content::IsPreTest()
                                   ? GetParam().metrics_expectation_before
                                   : GetParam().metrics_expectation_after;
    switch (metrics_expectation) {
      case kNotChecked:
        break;
      case kOSCryptAsyncMetrics:
        histogram_tester_.ExpectTotalCount("OSCrypt.AsyncInitialization.Time",
                                           1);
#if BUILDFLAG(IS_WIN)
        histogram_tester_.ExpectBucketCount("OSCrypt.DPAPIProvider.Status",
                                            /*success*/ 0, 1);
#endif  // BUILDFLAG(IS_WIN)
        break;
#if BUILDFLAG(IS_WIN)
      case kAppBoundEncryptMetrics:
        // In the pre-test the generation of a new key happens, followed by an
        // Encrypt.
        histogram_tester_.ExpectBucketCount(
            "OSCrypt.AppBoundProvider.KeyRetrieval.Status", /*kKeyNotFound*/ 1,
            1);
        histogram_tester_.ExpectBucketCount(
            "OSCrypt.AppBoundProvider.Encrypt.ResultCode", S_OK, 1);
        histogram_tester_.ExpectTotalCount(
            "OSCrypt.AppBoundProvider.Encrypt.ResultLastError", 0);
        histogram_tester_.ExpectTotalCount(
            "OSCrypt.AppBoundProvider.Decrypt.ResultCode", 0);
        break;
      case kAppBoundDecryptMetrics:
        histogram_tester_.ExpectBucketCount(
            "OSCrypt.AppBoundProvider.KeyRetrieval.Status", /*kSuccess*/ 0, 1);
        histogram_tester_.ExpectBucketCount(
            "OSCrypt.AppBoundProvider.Decrypt.ResultCode", S_OK, 1);
        histogram_tester_.ExpectTotalCount(
            "OSCrypt.AppBoundProvider.Decrypt.ResultLastError", 0);
        histogram_tester_.ExpectTotalCount(
            "OSCrypt.AppBoundProvider.Encrypt.ResultCode", 0);
        break;
#endif  // BUILDFLAG(IS_WIN)
      case kNoMetrics:
        histogram_tester_.ExpectTotalCount(
            "OSCrypt.AppBoundProvider.Decrypt.ResultCode", 0);
        histogram_tester_.ExpectTotalCount(
            "OSCrypt.AppBoundProvider.Encrypt.ResultCode", 0);
        break;
    }

    InProcessBrowserTest::TearDown();
  }

 private:
#if BUILDFLAG(IS_WIN)
  install_static::ScopedInstallDetails scoped_install_details_;
#endif  // BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
#if BUILDFLAG(IS_WIN)
  std::optional<base::ScopedClosureRunner> maybe_uninstall_service_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
#endif  // BUILDFLAG(IS_WIN)
};

IN_PROC_BROWSER_TEST_P(CookieEncryptionProviderBrowserTest, PRE_CookieStorage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/setcookie.html")));
}

IN_PROC_BROWSER_TEST_P(CookieEncryptionProviderBrowserTest, CookieStorage) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  base::test::TestFuture<const net::CookieList&> future;
  cookie_manager->GetAllCookies(future.GetCallback());
  auto cookies = future.Take();
  if (GetParam().expect_pass) {
    ASSERT_EQ(cookies.size(), 1u);
    EXPECT_EQ(cookies[0].Name(), "name");
    EXPECT_EQ(cookies[0].Value(), "Good");
  } else {
    ASSERT_TRUE(cookies.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieEncryptionProviderBrowserTest,
    testing::ValuesIn<TestCase>({
        {.name = "sync", .before = kOSCryptSync, .after = kOSCryptSync},
        {.name = "async",
         .before = kOSCryptAsync,
         .after = kOSCryptAsync,
         .metrics_expectation_before = kOSCryptAsyncMetrics,
         .metrics_expectation_after = kOSCryptAsyncMetrics},
        {.name = "migration_sync_to_async",
         .before = kOSCryptSync,
         .after = kOSCryptAsync,
         .metrics_expectation_after = kOSCryptAsyncMetrics},
        {.name = "rollback_async_to_sync",
         .before = kOSCryptAsync,
         .after = kOSCryptSync,
         .metrics_expectation_before = kOSCryptAsyncMetrics},
#if BUILDFLAG(IS_WIN)
        {.name = "migration_async_to_appbound_no_encryption",
         .before = kOSCryptAsync,
         .after = kOSCryptAsyncWithAppBoundProvider,
         .metrics_expectation_before = kOSCryptAsyncMetrics,
         .metrics_expectation_after = kAppBoundEncryptMetrics},
        {.name = "migration_async_to_appbound_with_encryption",
         .before = kOSCryptAsync,
         .after = kOSCryptAsyncWithAppBoundProviderWithEncryption,
         .metrics_expectation_before = kOSCryptAsyncMetrics,
         .metrics_expectation_after = kAppBoundEncryptMetrics},
        {.name = "rollback_turn_off_encryption_of_new_data",
         .before = kOSCryptAsyncWithAppBoundProviderWithEncryption,
         .after = kOSCryptAsyncWithAppBoundProvider,
         .metrics_expectation_before = kAppBoundEncryptMetrics,
         .metrics_expectation_after = kAppBoundDecryptMetrics},
        {.name = "app_bound_encryption_can_decrypt",
         .before = kOSCryptAsyncWithAppBoundProviderWithEncryption,
         .after = kOSCryptAsyncWithAppBoundProviderWithEncryption,
         .metrics_expectation_before = kAppBoundEncryptMetrics,
         .metrics_expectation_after = kAppBoundDecryptMetrics},
        {.name = "rollback_unregister_app_bound_provider",
         .before = kOSCryptAsyncWithAppBoundProvider,
         .after = kOSCryptAsync,
         .metrics_expectation_before = kAppBoundEncryptMetrics},
        // It is unsupported to move back from enabling app-bound encryption
        // provider with encryption, to a state where the provider is no longer
        // registered, but the test is here to verify all expectations match
        // reality.
        {.name = "invalid_rollback_turn_off_app_bound_provider_after_"
                 "encrypting_data",
         .expect_pass = false,
         .before = kOSCryptAsyncWithAppBoundProviderWithEncryption,
         .after = kOSCryptAsync},
        // This test will result in App-Bound not being able to provide a key,
        // so it will not be registered, and the cookies will instead be
        // encrypted with the second provider which is DPAPI, and then these can
        // successfully be decrypted.
        {.name = "app_bound_encryption_no_service_on_encrypt",
         .before = kOSCryptAsyncWithAppBoundProviderWithEncryptionNoService,
         .after = kOSCryptAsync},
        // This test will result in App-Bound being able to provide a key and
        // it's used for encryption, but in the second part of the test, since
        // the service does not exist it will not be able to decrypt it.
        {.name = "app_bound_encryption_no_service_on_decrypt",
         .expect_pass = false,
         .before = kOSCryptAsyncWithAppBoundProviderWithEncryption,
         .after = kOSCryptAsyncWithAppBoundProviderWithEncryptionNoService},
        // This test will result in App-Bound being able to provide a key and
        // it's used for encryption, and in the second part of the test, the
        // system will be 'unsupported' due to a custom user data dir provided
        // by the test framework, but still be able to decrypt data, since the
        // App-Bound verification passes and the user data is, in fact, the
        // same.
        {.name = "app_bound_encryption_not_supported_on_decrypt",
         .before = kOSCryptAsyncWithAppBoundProviderWithEncryption,
         .after =
             kOSCryptAsyncWithAppBoundProviderWithEncryptionUnsupportedUserData},
        // This test will result in App-Bound not being able to provide a key,
        // as the system is unsupported, so it will not be registered, and the
        // cookies will instead be encrypted with the second provider which is
        // DPAPI, and then these can successfully be decrypted when App-Bound is
        // not enabled.
        {.name = "app_bound_encryption_not_supported_on_encrypt",
         .before =
             kOSCryptAsyncWithAppBoundProviderWithEncryptionUnsupportedUserData,
         .after = kOSCryptAsync},
        // This test verifies that if App-Bound encryption is disabled by
        // policy, then the provider does not generate a key. This means any
        // data encrypted in the first stage of the test should decrypt using
        // just the DPAPI provider.
        {.name = "app_bound_encryption_disabled_by_policy",
         .before = kOSCryptAsyncWithAppBoundProviderDisabledByPolicy,
         .after = kOSCryptAsync,
         .metrics_expectation_before = kNoMetrics},
        // This test verifies that if App-Bound encryption is first enabled by
        // policy (the default), then subsequently disabled by policy, then the
        // key is still successfully registered as there might be data that was
        // previously encrypted using the key.
        {.name = "app_bound_encryption_disabled_by_policy_later",
         .before = kOSCryptAsyncWithAppBoundProvider,
         .after = kOSCryptAsyncWithAppBoundProviderDisabledByPolicy,
         .metrics_expectation_before = kAppBoundEncryptMetrics,
         .metrics_expectation_after = kAppBoundDecryptMetrics},
#endif  // BUILDFLAG(IS_WIN)
    }),
    [](const auto& info) { return info.param.name; });
