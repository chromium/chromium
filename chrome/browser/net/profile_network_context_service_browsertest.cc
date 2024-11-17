// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/profile_network_context_service_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/disk_cache/cache_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_preferences.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_server_handler_registration.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

// Most tests for this class are in NetworkContextConfigurationBrowserTest.
class ProfileNetworkContextServiceBrowsertest : public InProcessBrowserTest {
 public:
  ProfileNetworkContextServiceBrowsertest() = default;

  ~ProfileNetworkContextServiceBrowsertest() override = default;

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUpOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    loader_factory_ = browser()
                          ->profile()
                          ->GetDefaultStoragePartition()
                          ->GetURLLoaderFactoryForBrowserProcess()
                          .get();
  }

  network::mojom::URLLoaderFactory* loader_factory() const {
    return loader_factory_;
  }

  void CheckDiskCacheSizeHistogramRecorded() {
    std::string all_metrics;
    do {
      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
      base::PlatformThread::Sleep(base::Milliseconds(5));
      all_metrics = histograms_.GetAllHistogramsRecorded();
    } while (std::string::npos ==
             all_metrics.find("HttpCache.MaxFileSizeOnInit"));
  }

  base::HistogramTester histograms_;

 protected:
  // The HttpCache is only created when a request is issued, thus we perform a
  // navigation to ensure that the http cache is initialized.
  void NavigateToCreateHttpCache() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/createbackend")));
  }

 private:
  raw_ptr<network::mojom::URLLoaderFactory> loader_factory_ = nullptr;
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
      loader_factory(), simple_loader_helper.GetCallbackDeprecated());
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
  cert_verifier::mojom::CertVerifierCreationParams
      cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  EXPECT_EQ(0, network_context_params.http_cache_max_size);

  CheckDiskCacheSizeHistogramRecorded();
}

class DiskCachesizeExperiment : public ProfileNetworkContextServiceBrowsertest {
 public:
  DiskCachesizeExperiment() = default;
  ~DiskCachesizeExperiment() override = default;

  void SetUp() override {
    std::map<std::string, std::string> field_trial_params;
    field_trial_params["percent_relative_size"] = "200";
    feature_list_.InitAndEnableFeatureWithParameters(
        disk_cache::kChangeDiskCacheSizeExperiment, field_trial_params);
    ProfileNetworkContextServiceBrowsertest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DiskCachesizeExperiment, ScaledCacheSize) {
  // We don't have a great way of directly checking that the disk cache has the
  // correct max size, but we can make sure that we set up our network context
  // params correctly and that the histogram is recorded.
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser()->profile());
  base::FilePath empty_relative_partition_path;
  network::mojom::NetworkContextParams network_context_params;
  cert_verifier::mojom::CertVerifierCreationParams
      cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  EXPECT_EQ(0, network_context_params.http_cache_max_size);

  CheckDiskCacheSizeHistogramRecorded();
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
      loader_factory(), simple_loader_helper.GetCallbackDeprecated());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());
  std::vector<std::string> encodings =
      base::SplitString(*simple_loader_helper.response_body(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  EXPECT_TRUE(base::Contains(encodings, "br"));
}

void CheckCacheResetStatus(base::HistogramTester* histograms, bool reset) {
  // TODO(crbug.com/40114587): The failure case, here, is to time out.  Since
  // Chrome doesn't synchronize cache loading, there's no guarantee that this is
  // complete and it's merely available at earliest convenience.  If shutdown
  // occurs prior to the cache being loaded, then nothing is reported.  This
  // should probably be fixed to avoid the use of the sleep function, but that
  // will require synchronizing in some meaningful way to guarantee the cache
  // has been loaded prior to testing the histograms.
  while (!histograms->GetBucketCount("HttpCache.HardReset", reset)) {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::PlatformThread::Sleep(base::Milliseconds(5));
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
  ProfileNetworkContextServiceCacheSameBrowsertest() {
    // Override features that are enabled via the fieldtrial testing config.
    split_cache_features_disabled_feature_list_.InitWithFeatures(
        {}, {
                net::features::kSplitCacheByNetworkIsolationKey,
                net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean,
                net::features::kSplitCacheByMainFrameNavigationInitiator,
                net::features::kSplitCacheByNavigationInitiator,
            });
  }
  ~ProfileNetworkContextServiceCacheSameBrowsertest() override = default;

  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList split_cache_features_disabled_feature_list_;
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
      "None None None None");
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
      "None None None None");
}

class ProfileNetworkContextServiceCacheChangeBrowsertest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  ProfileNetworkContextServiceCacheChangeBrowsertest() {
    split_cache_always_enabled_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kSplitCacheByNetworkIsolationKey, {});

    split_cache_experiment_always_disabled_feature_list_.InitWithFeatures(
        {}, {
                net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean,
                net::features::kSplitCacheByMainFrameNavigationInitiator,
                net::features::kSplitCacheByNavigationInitiator,
            });
  }
  ~ProfileNetworkContextServiceCacheChangeBrowsertest() override = default;

  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList split_cache_always_enabled_feature_list_;
  base::test::ScopedFeatureList
      split_cache_experiment_always_disabled_feature_list_;
};

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
      "scoped_feature_list_trial_group None None None");
  // Set the local state for the next test.
  local_state->SetString(
      "profile_network_context_service.http_cache_finch_experiment_groups",
      "None None None None");
}

// The second time we load we know the state, which was "None None None None"
// for the previous test, so we should see a reset being in an experiment.
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
      "scoped_feature_list_trial_group None None None");
}

// This subclass adds the "SplitCacheByIncludeCredentials" feature.
class ProfileNetworkContextServiceCacheCredentialsBrowserTest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  ProfileNetworkContextServiceCacheCredentialsBrowserTest() {
    split_cache_always_enabled_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kSplitCacheByIncludeCredentials, {});

    split_cache_experiment_always_disabled_feature_list_.InitWithFeatures(
        {}, {
                net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean,
                net::features::kSplitCacheByMainFrameNavigationInitiator,
                net::features::kSplitCacheByNavigationInitiator,
            });
  }
  ~ProfileNetworkContextServiceCacheCredentialsBrowserTest() override = default;

  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList split_cache_always_enabled_feature_list_;
  base::test::ScopedFeatureList
      split_cache_experiment_always_disabled_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceCacheCredentialsBrowserTest,
                       PRE_TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, false);

  // At this point, we have already called the initialization.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      "None None None scoped_feature_list_trial_group");
  // Set the local state for the next test.
  local_state->SetString(
      "profile_network_context_service.http_cache_finch_experiment_groups",
      "None None None None");
}

// The second time we load we know the state, which was "None None None None"
// for the previous test, so we should see a reset being in an experiment.
IN_PROC_BROWSER_TEST_F(ProfileNetworkContextServiceCacheCredentialsBrowserTest,
                       TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, true);

  // At this point, we have already called the initialization once.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      "None None None scoped_feature_list_trial_group");
}

// This subclass adds tests for the 2024 HTTP Cache keying experiment flags.
enum class HttpCache2024ExperimentTestCase {
  kEnabledTriplePlusCrossSiteMainFrameNavBool,
  kEnabledTriplePlusMainFrameNavInitiator,
  kEnabledTriplePlusNavInitiator,
  kControlGroup
};

const struct {
  const HttpCache2024ExperimentTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {HttpCache2024ExperimentTestCase::
         kEnabledTriplePlusCrossSiteMainFrameNavBool,
     net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
    {HttpCache2024ExperimentTestCase::kEnabledTriplePlusMainFrameNavInitiator,
     net::features::kSplitCacheByMainFrameNavigationInitiator},
    {HttpCache2024ExperimentTestCase::kEnabledTriplePlusNavInitiator,
     net::features::kSplitCacheByNavigationInitiator},
    {HttpCache2024ExperimentTestCase::kControlGroup,
     net::features::kHttpCacheKeyingExperimentControlGroup2024}};

class ProfileNetworkContextServiceCacheKeySchemeExperimentBrowserTest
    : public ProfileNetworkContextServiceBrowsertest,
      public testing::WithParamInterface<HttpCache2024ExperimentTestCase> {
 public:
  ProfileNetworkContextServiceCacheKeySchemeExperimentBrowserTest()
      : split_cache_experiment_feature_list_(GetParam(),
                                             kTestCaseToFeatureMapping) {
    // Override any configured experiments for the
    // SplitCacheByNetworkIsolationKey feature.
    split_cache_always_enabled_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kSplitCacheByNetworkIsolationKey, {});
  }

  const char* GetExperimentString() {
    switch (GetParam()) {
      case HttpCache2024ExperimentTestCase::
          kEnabledTriplePlusCrossSiteMainFrameNavBool:
        return "20240814-CrossSiteNavBool";
      case HttpCache2024ExperimentTestCase::
          kEnabledTriplePlusMainFrameNavInitiator:
        return "20240814-MainFrameNavigationInitiator";
      case HttpCache2024ExperimentTestCase::kEnabledTriplePlusNavInitiator:
        return "20240814-NavigationInitiator";
      case HttpCache2024ExperimentTestCase::kControlGroup:
        return "20240814-ExperimentControlGroup";
    }
  }

  base::HistogramTester histograms_;

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
  base::test::ScopedFeatureList split_cache_always_enabled_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfileNetworkContextServiceCacheKeySchemeExperimentBrowserTest,
    testing::ValuesIn(
        {HttpCache2024ExperimentTestCase::
             kEnabledTriplePlusCrossSiteMainFrameNavBool,
         HttpCache2024ExperimentTestCase::
             kEnabledTriplePlusMainFrameNavInitiator,
         HttpCache2024ExperimentTestCase::kEnabledTriplePlusNavInitiator,
         HttpCache2024ExperimentTestCase::kControlGroup}),
    [](const testing::TestParamInfo<HttpCache2024ExperimentTestCase>& info) {
      switch (info.param) {
        case HttpCache2024ExperimentTestCase::
            kEnabledTriplePlusCrossSiteMainFrameNavBool:
          return "SplitCacheEnabledTriplePlusCrossSiteMainFrameNavigationBool";
        case HttpCache2024ExperimentTestCase::
            kEnabledTriplePlusMainFrameNavInitiator:
          return "SplitCacheEnabledTriplePlusMainFrameNavigationInitiator";
        case HttpCache2024ExperimentTestCase::kEnabledTriplePlusNavInitiator:
          return "SplitCacheEnabledTriplePlusNavigationInitiator";
        case (HttpCache2024ExperimentTestCase::kControlGroup):
          return "ControlGroup";
      }
    });

IN_PROC_BROWSER_TEST_P(
    ProfileNetworkContextServiceCacheKeySchemeExperimentBrowserTest,
    PRE_TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, false);

  // At this point, we have already called the initialization.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      base::StrCat({"scoped_feature_list_trial_group None None None ",
                    GetExperimentString()}));
  // Set the local state for the next test.
  local_state->SetString(
      "profile_network_context_service.http_cache_finch_experiment_groups",
      "None None None None");
}

// The second time we load we know the state, which was "None None None None"
// for the previous test, so we should see a reset being in an experiment.
IN_PROC_BROWSER_TEST_P(
    ProfileNetworkContextServiceCacheKeySchemeExperimentBrowserTest,
    TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, true);

  // At this point, we have already called the initialization once.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(
      local_state->GetString(
          "profile_network_context_service.http_cache_finch_experiment_groups"),
      base::StrCat({"scoped_feature_list_trial_group None None None ",
                    GetExperimentString()}));
}

class ProfileNetworkContextServiceDiskCacheBackendExperimentBrowserTest
    : public ProfileNetworkContextServiceBrowsertest,
      public ::testing::WithParamInterface<net::features::DiskCacheBackend> {
 public:
  ProfileNetworkContextServiceDiskCacheBackendExperimentBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kDiskCacheBackendExperiment,
        {{"backend", GetBackendParamValue()}});
  }
  ~ProfileNetworkContextServiceDiskCacheBackendExperimentBrowserTest()
      override = default;

  const char* GetBackendParamValue() {
    switch (GetParam()) {
      case net::features::DiskCacheBackend::kSimple:
        return "simple";
      case net::features::DiskCacheBackend::kBlockfile:
        return "blockfile";
    }
  }

  const char* GetExperimentString() {
    switch (GetParam()) {
      case net::features::DiskCacheBackend::kSimple:
        return "20241007-DiskCache-Simple";
      case net::features::DiskCacheBackend::kBlockfile:
        return "20241007-DiskCache-Blockfile";
    }
  }

  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    ProfileNetworkContextServiceDiskCacheBackendExperimentBrowserTest,
    PRE_TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, false);

  // At this point, we have already called the initialization.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(local_state->GetString("profile_network_context_service.http_"
                                   "cache_finch_experiment_groups"),
            base::StrCat({"None None None None ", GetExperimentString()}));

  // Set the local state for the next test.
  local_state->SetString(
      "profile_network_context_service.http_cache_finch_experiment_groups",
      "None None None None");
}

// The second time we load we know the state, which was "None None None None"
// for the previous test, so we should see a reset being in an experiment.
IN_PROC_BROWSER_TEST_P(
    ProfileNetworkContextServiceDiskCacheBackendExperimentBrowserTest,
    TestCacheResetParameter) {
  NavigateToCreateHttpCache();
  CheckCacheResetStatus(&histograms_, true);

  // At this point, we have already called the initialization.
  // Verify that we have the correct values in the local_state.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK_EQ(local_state->GetString("profile_network_context_service.http_"
                                   "cache_finch_experiment_groups"),
            base::StrCat({"None None None None ", GetExperimentString()}));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfileNetworkContextServiceDiskCacheBackendExperimentBrowserTest,
    testing::ValuesIn({net::features::DiskCacheBackend::kSimple,
                       net::features::DiskCacheBackend::kBlockfile}));

class AmbientAuthenticationTestWithPolicy : public policy::PolicyTest {
 public:
  AmbientAuthenticationTestWithPolicy() {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void IsAmbientAuthAllowedForProfilesTest() {
    PrefService* service = g_browser_process->local_state();
    int policy_value =
        service->GetInteger(prefs::kAmbientAuthenticationInPrivateModesEnabled);

    Profile* regular_profile = browser()->profile();
    Profile* incognito_profile =
        regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    Profile* non_primary_otr_profile = regular_profile->GetOffTheRecordProfile(
        Profile::OTRProfileID::CreateUniqueForTesting(),
        /*create_if_needed=*/true);

    EXPECT_TRUE(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
        regular_profile));
    EXPECT_TRUE(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
        non_primary_otr_profile));
    EXPECT_EQ(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
                  incognito_profile),
              AmbientAuthenticationTestHelper::IsIncognitoAllowedInPolicy(
                  policy_value));
// ChromeOS guest sessions don't have the capability to
// do ambient authentications.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_EQ(
        AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
            CreateGuestBrowser()->profile()),
        AmbientAuthenticationTestHelper::IsGuestAllowedInPolicy(policy_value));
#endif
  }

  void EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes value) {
    SetPolicy(&policies_,
              policy::key::kAmbientAuthenticationInPrivateModesEnabled,
              base::Value(static_cast<int>(value)));
    UpdateProviderPolicy(policies_);
  }

 private:
  policy::PolicyMap policies_;
};

IN_PROC_BROWSER_TEST_F(AmbientAuthenticationTestWithPolicy, RegularOnly) {
  EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes::kRegularOnly);
  IsAmbientAuthAllowedForProfilesTest();
}

IN_PROC_BROWSER_TEST_F(AmbientAuthenticationTestWithPolicy,
                       IncognitoAndRegular) {
  EnablePolicyWithValue(
      net::AmbientAuthAllowedProfileTypes::kIncognitoAndRegular);
  IsAmbientAuthAllowedForProfilesTest();
}

IN_PROC_BROWSER_TEST_F(AmbientAuthenticationTestWithPolicy, GuestAndRegular) {
  EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes::kGuestAndRegular);
  IsAmbientAuthAllowedForProfilesTest();
}

IN_PROC_BROWSER_TEST_F(AmbientAuthenticationTestWithPolicy, All) {
  EnablePolicyWithValue(net::AmbientAuthAllowedProfileTypes::kAll);
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
      loader_factory(), simple_loader_helper.GetCallbackDeprecated());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());

  // Cache directory should now exist.
  base::FilePath expected_cache_path =
      TempPath()
          .Append(browser()->profile()->GetBaseName())
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
  cert_verifier::mojom::CertVerifierCreationParams
      cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  EXPECT_EQ(kCacheSize, network_context_params.http_cache_max_size);
}

#if BUILDFLAG(IS_CHROMEOS)
class ProfileNetworkContextServiceMemoryPressureFeatureBrowsertest
    : public ProfileNetworkContextServiceBrowsertest,
      public ::testing::WithParamInterface<std::optional<bool>> {
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
  cert_verifier::mojom::CertVerifierCreationParams
      cert_verifier_creation_params;
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
    ::testing::Values(std::nullopt, true, false));
#endif  // BUILDFLAG(IS_CHROMEOS)

class ProfileNetworkContextTrustTokensBrowsertest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  ProfileNetworkContextTrustTokensBrowsertest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    feature_list_.InitWithFeaturesAndParameters(
        // Enabled Features:
        {{network::features::kPrivateStateTokens,
          {{field_trial_param.name,
            field_trial_param.GetName(
                network::features::TrustTokenOriginTrialSpec::
                    kOriginTrialNotRequired)}}}},
        // Disabled Features:
        {});
  }
  ~ProfileNetworkContextTrustTokensBrowsertest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    network::test::RegisterTrustTokenTestHandlers(https_server_.get(),
                                                  &request_handler_);
    ASSERT_TRUE(https_server_->Start());
  }

  void ProvideRequestHandlerKeyCommitmentsToNetworkService(
      std::string_view host) {
    base::flat_map<url::Origin, std::string_view> origins_and_commitments;
    std::string key_commitments = request_handler_.GetKeyCommitmentRecord();

    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    origins_and_commitments.insert_or_assign(
        url::Origin::Create(
            https_server_->base_url().ReplaceComponents(replacements)),
        key_commitments);

    base::RunLoop run_loop;
    content::GetNetworkService()->SetTrustTokenKeyCommitments(
        network::WrapKeyCommitmentsForIssuers(
            std::move(origins_and_commitments)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void Flush() {
    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->FlushNetworkInterfaceForTesting();
  }

 protected:
  net::EmbeddedTestServer* https_test_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  network::test::TrustTokenRequestHandler request_handler_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileNetworkContextTrustTokensBrowsertest,
                       TrustTokenBlocked) {
  base::test::ScopedFeatureList feature_list_;
  ProvideRequestHandlerKeyCommitmentsToNetworkService("a.test");
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(browser()->profile());
  auto privacy_sandbox_delegate = std::make_unique<
      privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
  privacy_sandbox_delegate->SetUpIsPrivacySandboxRestrictedResponse(
      /*restricted=*/false);
  privacy_sandbox_delegate->SetUpIsIncognitoProfileResponse(
      /*incognito=*/browser()->profile()->IsIncognitoProfile());
  privacy_sandbox_settings->SetDelegateForTesting(
      std::move(privacy_sandbox_delegate));
  privacy_sandbox_settings->SetAllPrivacySandboxAllowedForTesting();
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  Flush();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("a.test", "/title1.html")));

  std::string issuance_origin =
      url::Origin::Create(https_test_server()->GetURL("a.test", "/"))
          .Serialize();

  std::string command = content::JsReplace(R"(
  (async () => {
    try {
      await fetch("/issue", {privateToken: {version: 1,
                                          operation: 'token-request'}});
      return await document.hasPrivateToken($1);
    } catch {
      return false;
    }
  })();)",
                                           issuance_origin);

  EXPECT_EQ(true, EvalJs(GetActiveWebContents(), command));

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::ANTI_ABUSE, CONTENT_SETTING_BLOCK);
  Flush();

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));
  EXPECT_EQ(false, EvalJs(GetActiveWebContents(), command));

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::ANTI_ABUSE, CONTENT_SETTING_ALLOW);
  Flush();

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));
  EXPECT_EQ(true, EvalJs(GetActiveWebContents(), command));

  // Trust Tokens are blocked when the top level origin cookie content setting
  // is blocked
  GURL top_level_origin = https_test_server()->GetURL("a.test", "/");
  host_content_settings_map->SetContentSettingDefaultScope(
      top_level_origin, top_level_origin, ContentSettingsType::COOKIES,
      CONTENT_SETTING_BLOCK);

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));
  EXPECT_EQ(false, EvalJs(GetActiveWebContents(), command));
}

class ReportingEndpointsPolicyTest : public policy::PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kReportingApiEnableEnterpriseCookieIssues);
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void UpdateReportingEndpointsPolicy(base::Value::Dict dict) {
    SetPolicy(&policies_, policy::key::kReportingEndpoints,
              base::Value(std::move(dict)));
    UpdateProviderPolicy(policies_);
  }

 private:
  policy::PolicyMap policies_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReportingEndpointsPolicyTest,
                       CheckEnterpriseEndpointsNetworkContextParamsSet) {
  network::mojom::NetworkContextParams network_context_params;
  EXPECT_FALSE(
      network_context_params.enterprise_reporting_endpoints.has_value());
  UpdateReportingEndpointsPolicy(
      base::Value::Dict()
          .Set("endpoint-1", "https://example.com/reports")
          .Set("endpoint-2", "https://reporting.example/cookie-issues")
          .Set("endpoint-3", "https://report-collector.example"));
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser()->profile());
  base::FilePath empty_relative_partition_path;
  cert_verifier::mojom::CertVerifierCreationParams
      cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  base::flat_map<std::string, GURL> expected_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };
  EXPECT_EQ(expected_enterprise_endpoints,
            network_context_params.enterprise_reporting_endpoints);
}
