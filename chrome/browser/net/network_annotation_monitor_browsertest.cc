// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_annotation_monitor.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {

enum class TestCase {
  kEnabled,
  kDisabled,
};

// Network annotation that will trigger a violation. Note: This annotation is
// currently hard-coded as a "disabled" annotation in the feature itself. In the
// future, this test should set specific policy values to disable this
// annotation.
constexpr net::NetworkTrafficAnnotationTag kTestDisabledAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_query", "");

class NetworkAnnotationMonitorBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<TestCase> {
 public:
  NetworkAnnotationMonitorBrowserTest() {
    if (GetParam() == TestCase::kEnabled) {
      feature_list_.InitAndEnableFeature(
          features::kNetworkAnnotationMonitoring);
    }
  }

  void SendFakeNetworkRequest(net::NetworkTrafficAnnotationTag annotation) {
    auto request = std::make_unique<network::ResourceRequest>();

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request), annotation);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        browser()
            ->profile()
            ->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(NetworkAnnotationMonitorBrowserTest, FeatureTest) {
  base::HistogramTester histogram_tester;

  // Send network request with arbitrary network annotation that will never
  // trigger a violation.
  SendFakeNetworkRequest(TRAFFIC_ANNOTATION_FOR_TESTS);
  // Send network request with a network annotation that will trigger a
  // violation.
  SendFakeNetworkRequest(kTestDisabledAnnotation);

  g_browser_process->system_network_context_manager()
      ->FlushNetworkAnnotationMonitorForTesting();

  // Disabled hash codes should trigger a violation only if the feature is
  // enabled.
  int expected_count = GetParam() == TestCase::kEnabled ? 1 : 0;
  histogram_tester.ExpectBucketCount(
      "NetworkAnnotationMonitor.PolicyViolation",
      kTestDisabledAnnotation.unique_id_hash_code, expected_count);

  // Other hash codes should never trigger a violation, regardless of feature
  // state.
  histogram_tester.ExpectBucketCount(
      "NetworkAnnotationMonitor.PolicyViolation",
      TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code, 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         NetworkAnnotationMonitorBrowserTest,
                         ::testing::Values(TestCase::kDisabled,
                                           TestCase::kEnabled));

}  // namespace
