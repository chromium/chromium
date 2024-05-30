// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_annotation_monitor.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/regmon/regmon_client.h"
#include "components/policy/policy_constants.h"
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

// Network annotation that can trigger a violation.
constexpr net::NetworkTrafficAnnotationTag kTestDisabledAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_query", "");
// Policy that controls the above annotation. When set to false, any occurrences
// of the above annotation should trigger a violation.
constexpr char kTestPolicy[] = "PasswordManagerEnabled";

class NetworkAnnotationMonitorBrowserTest
    : public policy::PolicyTest,
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
  policy::PolicyMap policies;
  SetPolicy(&policies, kTestPolicy, base::Value(false));
  // Disable secondary profiles policy since we skip reporting on lacros when
  // this is enabled.
  SetPolicy(&policies, policy::key::kLacrosSecondaryProfilesAllowed,
            base::Value(false));
  provider_.UpdateChromePolicy(policies);

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
  chromeos::RegmonClient* regmon_client = chromeos::RegmonClient::Get();
  if (GetParam() == TestCase::kEnabled) {
    std::list<int32_t> expected_hash_codes{
        kTestDisabledAnnotation.unique_id_hash_code};
    EXPECT_EQ(regmon_client->GetTestInterface()->GetReportedHashCodes(),
              expected_hash_codes);
  } else {
    std::list<int32_t> expected_hash_codes{};
    EXPECT_EQ(regmon_client->GetTestInterface()->GetReportedHashCodes(),
              expected_hash_codes);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         NetworkAnnotationMonitorBrowserTest,
                         ::testing::Values(TestCase::kDisabled,
                                           TestCase::kEnabled));

}  // namespace
