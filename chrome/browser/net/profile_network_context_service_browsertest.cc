// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/profile_network_context_service_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/http/http_auth_preferences.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/cert_verifier/test_cert_verifier_service_factory.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

// Most tests for this class are in NetworkContextConfigurationBrowserTest.
class ProfileNetworkContextServiceBrowsertest : public InProcessBrowserTest {
 public:
  ProfileNetworkContextServiceBrowsertest() = default;

  ~ProfileNetworkContextServiceBrowsertest() override = default;

  void SetUpOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    loader_factory_ = content::BrowserContext::GetDefaultStoragePartition(
                          browser()->profile())
                          ->GetURLLoaderFactoryForBrowserProcess()
                          .get();
  }

  network::mojom::URLLoaderFactory* loader_factory() const {
    return loader_factory_;
  }

 protected:
  // The HttpCache is only created when a request is issued, thus we perform a
  // navigation to ensure that the http cache is initialized.
  void NavigateToCreateHttpCache() {
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/createbackend"));
  }

 private:
  network::mojom::URLLoaderFactory* loader_factory_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceBrowsertest,
                       DiskCacheLocation) {
  // Run a request that caches the response, to give the network service time to
  // create a cache directory.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/cachetime");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());

  base::FilePath expected_cache_path;
  chrome::GetUserCacheDirectory(browser()->profile()->GetPath(),
                                &expected_cache_path);
  expected_cache_path = expected_cache_path.Append(chrome::kCacheDirname);
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(expected_cache_path));
}

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceBrowsertest,
                       DefaultCacheSize) {
  // We don't have a great way of directly checking that the disk cache has the
  // correct max size, but we can make sure that we set up our network context
  // params correctly.
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser()->profile());
  base::FilePath empty_relative_partition_path;
  network::mojom::NetworkContextParams network_context_params;
  network::mojom::CertVerifierCreationParams cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  EXPECT_EQ(0, network_context_params.http_cache_max_size);
}

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceBrowsertest, BrotliEnabled) {
  // Brotli is only used over encrypted connections.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = https_server.GetURL("/echoheader?accept-encoding");

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());
  std::vector<std::string> encodings =
      base::SplitString(*simple_loader_helper.response_body(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  EXPECT_TRUE(base::Contains(encodings, "br"));
}

void CheckCacheResetStatus(base::HistogramTester* histograms, bool reset) {
  // TODO(crbug/1041810): The failure case, here, is to time out.  Since Chrome
  // doesn't synchronize cache loading, there's no guarantee that this is
  // complete and it's merely available at earliest convenience.  If shutdown
  // occurs prior to the cache being loaded, then nothing is reported.  This
  // should probably be fixed to avoid the use of the sleep function, but that
  // will require synchronizing in some meaningful way to guarantee the cache
  // has been loaded prior to testing the histograms.
  while (!histograms->GetBucketCount("HttpCache.HardReset", reset)) {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(5));
  }

  if (reset) {
    // Some tests load the cache multiple times, but should only be reset once.
    EXPECT_EQ(histograms->GetBucketCount("HttpCache.HardReset", true), 1);
  } else {
    // Make sure it's never reset.
    EXPECT_EQ(histograms->GetBucketCount("HttpCache.HardReset", true), 0);
  }
}

class ProfileNetworkContextServiceCacheSameBrowsertest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  ProfileNetworkContextServiceCacheSameBrowsertest() = default;
  ~ProfileNetworkContextServiceCacheSameBrowsertest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {net::features::kSplitCacheByNetworkIsolationKey,
             net::features::kAppendFrameOriginToNetworkIsolationKey});
    ProfileNetworkContextServiceBrowsertest::SetUp();
  }

  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceCacheSameBrowsertest,
                       PRE_TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, false);

  // At this point, we have already called the initialization.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      "None None None");
}

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceCacheSameBrowsertest,
                       TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, false);

  // At this point, we have already called the initialization.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      "None None None");
}

class ProfileNetworkContextServiceCacheChangeBrowsertest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  ProfileNetworkContextServiceCacheChangeBrowsertest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kAppendFrameOriginToNetworkIsolationKey, {}}},
        {net::features::kSplitCacheByNetworkIsolationKey});
  }
  ~ProfileNetworkContextServiceCacheChangeBrowsertest() override = default;

  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on Linux and Mac: https://crbug.com/1041810
// The first time we load, even if we're in an experiment there's no reset
// from the unknown state.
IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceCacheChangeBrowsertest,
                       PRE_TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, false);

  // At this point, we have already called the initialization.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      "None scoped_feature_list_trial_group None");
  // Set the local state for the next test.
  local_state->SetString(
      "profile_network_context_service.http_cache_finch_experiment_groups",
      "None None None");
}

// The second time we load we know the state, which was "None None None" for the
// previous test, so we should see a reset being in an experiment.
IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceCacheChangeBrowsertest,
                       TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, true);

  // At this point, we have already called the initialization once.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      "None scoped_feature_list_trial_group None");
}

class AmbientAuthenticationTestWithPolicy
    : public policy::PolicyTest,
      public testing::WithParamInterface<AmbientAuthenticationFeatureState> {
 public:
  AmbientAuthenticationTestWithPolicy() {
    feature_state_ = GetParam();
    AmbientAuthenticationTestHelper::CookTheFeatureList(scoped_feature_list_,
                                                        feature_state_);
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void IsAmbientAuthAllowedForProfilesTest() {
    PrefService* service = g_browser_process->local_state();
    int policy_value =
        service->GetInteger(prefs::kAmbientAuthenticationInPrivateModesEnabled);

    Profile* regular_profile = browser()->profile();
    Profile* incognito_profile = regular_profile->GetPrimaryOTRProfile();
    Profile* non_primary_otr_profile = regular_profile->GetOffTheRecordProfile(
        Profile::OTRProfileID("Test::AmbientAuthentication"));

    EXPECT_TRUE(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
        regular_profile));
    EXPECT_TRUE(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
        non_primary_otr_profile));
    EXPECT_EQ(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
                  incognito_profile),
              AmbientAuthenticationTestHelper::IsIncognitoAllowedInFeature(
                  feature_state_) ||
                  AmbientAuthenticationTestHelper::IsIncognitoAllowedInPolicy(
                      policy_value));
// ChromeOS guest sessions don't have the capability to
// do ambient authentications.
#if !defined(OS_CHROMEOS)
    EXPECT_EQ(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
                  AmbientAuthenticationTestHelper::GetGuestProfile()),
              AmbientAuthenticationTestHelper::IsGuestAllowedInFeature(
                  feature_state_) ||
                  AmbientAuthenticationTestHelper::IsGuestAllowedInPolicy(
                      policy_value));
#endif
  }

  void EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes value) {
    SetPolicy(&policies_,
              policy::key::kAmbientAuthenticationInPrivateModesEnabled,
              base::Value(static_cast<int>(value)));
    UpdateProviderPolicy(policies_);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  AmbientAuthenticationFeatureState feature_state_;
  policy::PolicyMap policies_;
};

INSTANTIATE_TEST_CASE_P(
    AmbientAuthAllFeatureValuesTest,
    AmbientAuthenticationTestWithPolicy,
    testing::Values(AmbientAuthenticationFeatureState::GUEST_OFF_INCOGNITO_OFF,
                    AmbientAuthenticationFeatureState::GUEST_OFF_INCOGNITO_ON,
                    AmbientAuthenticationFeatureState::GUEST_ON_INCOGNITO_OFF,
                    AmbientAuthenticationFeatureState::GUEST_ON_INCOGNITO_ON));

IN_PROC_BROWSER_TEST_P(AmbientAuthenticationTestWithPolicy, RegularOnly) {
  EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes::REGULAR_ONLY);
  IsAmbientAuthAllowedForProfilesTest();
}

IN_PROC_BROWSER_TEST_P(AmbientAuthenticationTestWithPolicy,
                       IncognitoAndRegular) {
  EnablePolicyWithValue(
      net::AmbientAuthAllowedProfileTypes::INCOGNITO_AND_REGULAR);
  IsAmbientAuthAllowedForProfilesTest();
}

IN_PROC_BROWSER_TEST_P(AmbientAuthenticationTestWithPolicy, GuestAndRegular) {
  EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes::GUEST_AND_REGULAR);
  IsAmbientAuthAllowedForProfilesTest();
}

IN_PROC_BROWSER_TEST_P(AmbientAuthenticationTestWithPolicy, All) {
  EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes::ALL);
  IsAmbientAuthAllowedForProfilesTest();
}

// Test subclass that adds switches::kDiskCacheDir and switches::kDiskCacheSize
// to the command line, to make sure they're respected.
class ProfileNetworkContextServiceDiskCacheBrowsertest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  const int64_t kCacheSize = 7;

  ProfileNetworkContextServiceDiskCacheBrowsertest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ~ProfileNetworkContextServiceDiskCacheBrowsertest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchPath(switches::kDiskCacheDir,
                                   temp_dir_.GetPath());
    command_line->AppendSwitchASCII(switches::kDiskCacheSize,
                                    base::NumberToString(kCacheSize));
  }

  const base::FilePath& TempPath() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Makes sure switches::kDiskCacheDir is hooked up correctly.
IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceDiskCacheBrowsertest,
                       DiskCacheLocation) {
  // Make sure command line switch is hooked up to the pref.
  ASSERT_EQ(TempPath(), g_browser_process->local_state()->GetFilePath(
                            prefs::kDiskCacheDir));

  // Run a request that caches the response, to give the network service time to
  // create a cache directory.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/cachetime");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());

  // Cache directory should now exist.
  base::FilePath expected_cache_path =
      TempPath()
          .Append(browser()->profile()->GetPath().BaseName())
          .Append(chrome::kCacheDirname);
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(expected_cache_path));
}

// Makes sure switches::kDiskCacheSize is hooked up correctly.
IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceDiskCacheBrowsertest,
                       DiskCacheSize) {
  // Make sure command line switch is hooked up to the pref.
  ASSERT_EQ(kCacheSize, g_browser_process->local_state()->GetInteger(
                            prefs::kDiskCacheSize));

  // We don't have a great way of directly checking that the disk cache has the
  // correct max size, but we can make sure that we set up our network context
  // params correctly.
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser()->profile());
  base::FilePath empty_relative_partition_path;
  network::mojom::NetworkContextParams network_context_params;
  network::mojom::CertVerifierCreationParams cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  EXPECT_EQ(kCacheSize, network_context_params.http_cache_max_size);
}

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
namespace {
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}
}  // namespace

class ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    std::vector<base::Feature> enabled_features, disabled_features;
    if (use_builtin_cert_verifier()) {
      enabled_features.push_back(net::features::kCertVerifierBuiltinFeature);
    } else {
      disabled_features.push_back(net::features::kCertVerifierBuiltinFeature);
    }
    if (enable_cert_verifier_service()) {
      enabled_features.push_back(network::features::kCertVerifierService);
      test_cert_verifier_service_factory_.emplace();
      content::SetCertVerifierServiceFactoryForTesting(
          &test_cert_verifier_service_factory_.value());
    } else {
      disabled_features.push_back(network::features::kCertVerifierService);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    content::SetCertVerifierServiceFactoryForTesting(nullptr);
  }

  void SetUpOnMainThread() override {
    if (enable_cert_verifier_service()) {
      test_cert_verifier_service_factory_->ReleaseAllCertVerifierParams();
    }
  }

  void ExpectUseBuiltinCertVerifierCorrectUsingCertVerifierService(
      network::mojom::CertVerifierCreationParams::CertVerifierImpl
          use_builtin_cert_verifier) {
    ASSERT_TRUE(enable_cert_verifier_service());
    ASSERT_TRUE(test_cert_verifier_service_factory_);
    EXPECT_EQ(1ul, test_cert_verifier_service_factory_->num_captured_params());
    EXPECT_EQ(use_builtin_cert_verifier,
              test_cert_verifier_service_factory_->GetParamsAtIndex(0)
                  ->creation_params->use_builtin_cert_verifier);
    // Send it to the actual CertVerifierServiceFactory.
    test_cert_verifier_service_factory_->ReleaseNextCertVerifierParams();
  }

  Profile* CreateNewProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        new_path, base::BindRepeating(&UnblockOnProfileCreation, &run_loop),
        base::string16(), std::string());
    run_loop.Run();
    return profile_manager->GetProfileByPath(new_path);
  }

  bool use_builtin_cert_verifier() const { return GetParam(); }
  bool enable_cert_verifier_service() const {
    return enable_cert_verifier_service_;
  }
  void set_enable_cert_verifier_service(bool enable_cv_service) {
    enable_cert_verifier_service_ = enable_cv_service;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool enable_cert_verifier_service_ = false;

  // Used if enable_cert_verifier_service() returns true.
  base::Optional<cert_verifier::TestCertVerifierServiceFactoryImpl>
      test_cert_verifier_service_factory_;
};

IN_PROC_BROWSER_TEST_P(
    ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTest,
    Test) {
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser()->profile());
  base::FilePath empty_relative_partition_path;
  {
    network::mojom::NetworkContextParams network_context_params;
    network::mojom::CertVerifierCreationParams cert_verifier_creation_params;
    profile_network_context_service->ConfigureNetworkContextParams(
        /*in_memory=*/false, empty_relative_partition_path,
        &network_context_params, &cert_verifier_creation_params);

    EXPECT_EQ(use_builtin_cert_verifier()
                  ? network::mojom::CertVerifierCreationParams::
                        CertVerifierImpl::kBuiltin
                  : network::mojom::CertVerifierCreationParams::
                        CertVerifierImpl::kSystem,
              cert_verifier_creation_params.use_builtin_cert_verifier);
  }

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
  // If the BuiltinCertificateVerifierEnabled policy is set it should override
  // the feature flag.
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kBuiltinCertificateVerifierEnabled,
            base::Value(true));
  UpdateProviderPolicy(policies);

  {
    network::mojom::NetworkContextParams network_context_params;
    network::mojom::CertVerifierCreationParams cert_verifier_creation_params;
    profile_network_context_service->ConfigureNetworkContextParams(
        /*in_memory=*/false, empty_relative_partition_path,
        &network_context_params, &cert_verifier_creation_params);
    EXPECT_EQ(
        network::mojom::CertVerifierCreationParams::CertVerifierImpl::kBuiltin,
        cert_verifier_creation_params.use_builtin_cert_verifier);
  }

  SetPolicy(&policies, policy::key::kBuiltinCertificateVerifierEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);

  {
    network::mojom::NetworkContextParams network_context_params;
    network::mojom::CertVerifierCreationParams cert_verifier_creation_params;
    profile_network_context_service->ConfigureNetworkContextParams(
        /*in_memory=*/false, empty_relative_partition_path,
        &network_context_params, &cert_verifier_creation_params);
    EXPECT_EQ(
        network::mojom::CertVerifierCreationParams::CertVerifierImpl::kSystem,
        cert_verifier_creation_params.use_builtin_cert_verifier);
  }
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTest,
    ::testing::Bool());

class
    ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTestWithService
    : public ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTest {
 public:
  ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTestWithService() {
    set_enable_cert_verifier_service(true);
  }

  ~ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTestWithService()
      override = default;
};

IN_PROC_BROWSER_TEST_P(
    ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTestWithService,
    Test) {
  {
    content::BrowserContext::GetDefaultStoragePartition(CreateNewProfile())
        ->GetNetworkContext();

    ExpectUseBuiltinCertVerifierCorrectUsingCertVerifierService(
        use_builtin_cert_verifier()
            ? network::mojom::CertVerifierCreationParams::CertVerifierImpl::
                  kBuiltin
            : network::mojom::CertVerifierCreationParams::CertVerifierImpl::
                  kSystem);
  }

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
  // If the BuiltinCertificateVerifierEnabled policy is set it should override
  // the feature flag.
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kBuiltinCertificateVerifierEnabled,
            base::Value(true));
  UpdateProviderPolicy(policies);

  {
    content::BrowserContext::GetDefaultStoragePartition(CreateNewProfile())
        ->GetNetworkContext();

    ExpectUseBuiltinCertVerifierCorrectUsingCertVerifierService(
        network::mojom::CertVerifierCreationParams::CertVerifierImpl::kBuiltin);
  }

  SetPolicy(&policies, policy::key::kBuiltinCertificateVerifierEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);

  {
    content::BrowserContext::GetDefaultStoragePartition(CreateNewProfile())
        ->GetNetworkContext();

    ExpectUseBuiltinCertVerifierCorrectUsingCertVerifierService(
        network::mojom::CertVerifierCreationParams::CertVerifierImpl::kSystem);
  }
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfileNetworkContextServiceCertVerifierBuiltinFeaturePolicyTestWithService,
    ::testing::Bool());
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)

#if defined(OS_CHROMEOS)
class ProfileNetworkContextServiceMemoryPressureFeatureBrowsertest
    : public ProfileNetworkContextServiceBrowsertest,
      public ::testing::WithParamInterface<base::Optional<bool>> {
 public:
  ProfileNetworkContextServiceMemoryPressureFeatureBrowsertest() = default;
  ~ProfileNetworkContextServiceMemoryPressureFeatureBrowsertest() override =
      default;

  void SetUp() override {
    if (GetParam().has_value()) {
      if (GetParam().value()) {
        scoped_feature_list_.InitWithFeatures(
            {chromeos::features::kDisableIdleSocketsCloseOnMemoryPressure}, {});
      } else {
        scoped_feature_list_.InitWithFeatures(
            {}, {chromeos::features::kDisableIdleSocketsCloseOnMemoryPressure});
      }
    }
    ProfileNetworkContextServiceBrowsertest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// If the feature is enabled (GetParam()==true),
// NetworkContextParams.disable_idle_sockets_close_on_memory_pressure is
// expected to be true.
// If the feature is not set or disabled (GetParam()==false or nullopt),
// NetworkContextParams.disable_idle_sockets_close_on_memory_pressure is
// expected to be false
IN_PROC_BROWSER_TEST_P(
    ProfileNetworkContextServiceMemoryPressureFeatureBrowsertest,
    FeaturePropagates) {
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser()->profile());
  base::FilePath empty_relative_partition_path;
  network::mojom::NetworkContextParams network_context_params;
  network::mojom::CertVerifierCreationParams cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  EXPECT_EQ(
      GetParam().value_or(false),
      network_context_params.disable_idle_sockets_close_on_memory_pressure);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfileNetworkContextServiceMemoryPressureFeatureBrowsertest,
    /*disable_idle_sockets_close_on_memory_pressure=*/
    ::testing::Values(base::nullopt, true, false));
#endif  // defined(OS_CHROMEOS)
