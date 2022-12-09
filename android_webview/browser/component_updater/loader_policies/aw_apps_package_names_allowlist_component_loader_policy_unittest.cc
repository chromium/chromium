// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/loader_policies/aw_apps_package_names_allowlist_component_loader_policy.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <utility>

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

using AllowlistPraseStatus =
    AwAppsPackageNamesAllowlistComponentLoaderPolicy::AllowlistPraseStatus;

namespace {

constexpr int kNumHash = 11;
constexpr int kNumBitsPerEntry = 16;
constexpr char kTestAllowlistVersion[] = "123.456.789.10";
const std::string kTestAllowlist[] = {"com.example.test", "my.fake.app",
                                      "yet.another.app"};
constexpr char kAllowlistPraseStatusHistogramName[] =
    "Android.WebView.Metrics.PackagesAllowList.ParseStatus";

double MillisFromUnixEpoch(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InMillisecondsF();
}

base::Value::Dict BuildTestManifest() {
  base::Value::Dict manifest;
  manifest.Set(kBloomFilterNumHashKey, base::Value(kNumHash));
  manifest.Set(kBloomFilterNumBitsKey, base::Value(3 * kNumBitsPerEntry));
  manifest.Set(
      kExpiryDateKey,
      base::Value(MillisFromUnixEpoch(base::Time::Now() + base::Days(1))));

  return manifest;
}

}  // namespace

class AwAppsPackageNamesAllowlistComponentLoaderPolicyTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&allowlist_path_));
  }

  void TearDown() override {
    base::DeleteFile(allowlist_path_);
  }

  void WriteAllowListToFile(const std::vector<uint8_t>& data) {
    ASSERT_TRUE(base::WriteFile(allowlist_path_, data));
  }

  void WritePackageNamesAllowListToFile() {
    auto filter = std::make_unique<optimization_guide::BloomFilter>(
        kNumHash, kNumBitsPerEntry * std::size(kTestAllowlist));
    for (const auto& package : kTestAllowlist) {
      filter->Add(package);
    }
    WriteAllowListToFile(filter->bytes());
  }

  base::ScopedFD OpenAndGetAllowlistFd() {
    int allowlist_fd = open(allowlist_path_.value().c_str(), O_RDONLY);
    CHECK(allowlist_fd) << "Failed to open FD for " << allowlist_path_;
    return base::ScopedFD(allowlist_fd);
  }

  void LookupConfirmationCallback(
      absl::optional<AppPackageNameLoggingRule> record) {
    EXPECT_TRUE(checker_.CalledOnValidSequence());
    allowlist_lookup_result_ = record;
    lookup_run_loop_.Quit();
  }

 protected:
  base::test::TaskEnvironment env_;
  // Has to be init after TaskEnvironment.
  base::SequenceCheckerImpl checker_;
  base::RunLoop lookup_run_loop_;
  base::HistogramTester histogram_tester_;

  absl::optional<AppPackageNameLoggingRule> allowlist_lookup_result_;

 private:
  base::FilePath allowlist_path_;
};

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestExistingPackageName) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  base::Value::Dict manifest = BuildTestManifest();
  base::Time one_day_from_now = base::Time::Now() + base::Days(1);
  manifest.Set(kExpiryDateKey, MillisFromUnixEpoch(one_day_from_now));
  base::Version new_version(kTestAllowlistVersion);

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1],
          AppPackageNameLoggingRule(base::Version("123.456.789.0"),
                                    base::Time::Min()),
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(new_version, fd_map, std::move(manifest));

  lookup_run_loop_.Run();
  ASSERT_TRUE(allowlist_lookup_result_.has_value());
  EXPECT_TRUE(allowlist_lookup_result_.value().IsAppPackageNameAllowed());
  EXPECT_EQ(allowlist_lookup_result_.value().GetVersion(), new_version);
  EXPECT_EQ(allowlist_lookup_result_.value().GetExpiryDate(), one_day_from_now);

  histogram_tester_.ExpectBucketCount(kAllowlistPraseStatusHistogramName,
                                      AllowlistPraseStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestSameVersionAsCache) {
  base::flat_map<std::string, base::ScopedFD> fd_map;
  base::Time one_day_from_now = base::Time::Now() + base::Days(1);
  base::Version version(kTestAllowlistVersion);

  AppPackageNameLoggingRule expected_record(version, one_day_from_now);
  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          "test.some.app", expected_record,
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(version, fd_map, BuildTestManifest());

  lookup_run_loop_.Run();
  ASSERT_TRUE(allowlist_lookup_result_.has_value());
  EXPECT_TRUE(allowlist_lookup_result_.value().IsAppPackageNameAllowed());
  EXPECT_TRUE(expected_record.IsSameAs(allowlist_lookup_result_.value()));

  histogram_tester_.ExpectBucketCount(kAllowlistPraseStatusHistogramName,
                                      AllowlistPraseStatus::kUsingCache, 1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestNonExistingPackageName) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  base::Version new_version(kTestAllowlistVersion);

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          "non.existent.app", absl::optional<AppPackageNameLoggingRule>(),
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(new_version, fd_map, BuildTestManifest());

  lookup_run_loop_.Run();
  ASSERT_TRUE(allowlist_lookup_result_.has_value());
  EXPECT_EQ(allowlist_lookup_result_.value().GetVersion(), new_version);
  EXPECT_FALSE(allowlist_lookup_result_.value().IsAppPackageNameAllowed());

  histogram_tester_.ExpectBucketCount(kAllowlistPraseStatusHistogramName,
                                      AllowlistPraseStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestAllowlistFileNotInMap) {
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map["another_file"] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1], absl::optional<AppPackageNameLoggingRule>(),
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(kTestAllowlistVersion), fd_map,
                          BuildTestManifest());

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_lookup_result_.has_value());

  histogram_tester_.ExpectBucketCount(
      kAllowlistPraseStatusHistogramName,
      AllowlistPraseStatus::kMissingAllowlistFile, 1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestMissingBloomFilterParams) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1], absl::optional<AppPackageNameLoggingRule>(),
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(kTestAllowlistVersion), fd_map,
                          /*manifest=*/base::Value::Dict());

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_lookup_result_.has_value());

  histogram_tester_.ExpectBucketCount(kAllowlistPraseStatusHistogramName,
                                      AllowlistPraseStatus::kMissingFields, 1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestTooShortBloomFilter) {
  WriteAllowListToFile(std::vector<uint8_t>(2, 0xff));
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1], absl::optional<AppPackageNameLoggingRule>(),
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(kTestAllowlistVersion), fd_map,
                          BuildTestManifest());

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_lookup_result_.has_value());

  histogram_tester_.ExpectBucketCount(
      kAllowlistPraseStatusHistogramName,
      AllowlistPraseStatus::kMalformedBloomFilter, 1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestTooLongBloomFilter) {
  WriteAllowListToFile(std::vector<uint8_t>(2000, 0xff));
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1], absl::optional<AppPackageNameLoggingRule>(),
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(kTestAllowlistVersion), fd_map,
                          BuildTestManifest());

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_lookup_result_.has_value());

  histogram_tester_.ExpectBucketCount(
      kAllowlistPraseStatusHistogramName,
      AllowlistPraseStatus::kMalformedBloomFilter, 1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestExpiredAllowlist) {
  WritePackageNamesAllowListToFile();
  base::flat_map<std::string, base::ScopedFD> fd_map;
  fd_map[kAllowlistBloomFilterFileName] = OpenAndGetAllowlistFd();
  base::Value::Dict manifest = BuildTestManifest();
  manifest.Set(
      kExpiryDateKey,
      base::Value(MillisFromUnixEpoch(base::Time::Now() - base::Days(1))));

  auto policy =
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          kTestAllowlist[1], absl::optional<AppPackageNameLoggingRule>(),
          base::BindOnce(&AwAppsPackageNamesAllowlistComponentLoaderPolicyTest::
                             LookupConfirmationCallback,
                         base::Unretained(this)));

  policy->ComponentLoaded(base::Version(kTestAllowlistVersion), fd_map,
                          std::move(manifest));

  lookup_run_loop_.Run();
  EXPECT_FALSE(allowlist_lookup_result_.has_value());

  histogram_tester_.ExpectBucketCount(kAllowlistPraseStatusHistogramName,
                                      AllowlistPraseStatus::kExpiredAllowlist,
                                      1);
  histogram_tester_.ExpectTotalCount(kAllowlistPraseStatusHistogramName, 1);
}

// Helper functions for throttling tests, defining them near tests body for
// better readability.
namespace {
class AwMetricsServiceClientTestDelegate
    : public AwMetricsServiceClient::Delegate {
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override {}
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override {}
  bool HasAwContentsEverCreated() const override { return false; }
};

void TestThrottling(base::Time time,
                    AwMetricsServiceClient* client,
                    bool expect_throttling) {
  if (!time.is_null()) {
    client->SetAppPackageNameLoggingRuleLastUpdateTime(time);
  }

  component_updater::ComponentLoaderPolicyVector policies;
  LoadPackageNamesAllowlistComponent(policies, client);
  EXPECT_EQ(policies.size(), expect_throttling ? 0 : 1u);
}

void TestThrottlingAllowlist(absl::optional<AppPackageNameLoggingRule> rule,
                             bool expect_throttling) {
  TestingPrefServiceSimple prefs;
  AwMetricsServiceClient::RegisterMetricsPrefs(prefs.registry());
  AwMetricsServiceClient client(
      std::make_unique<AwMetricsServiceClientTestDelegate>());
  client.Initialize(&prefs);
  client.SetAppPackageNameLoggingRule(rule);

  // No previous last_update record: should never throttle.
  TestThrottling(base::Time(), &client, /*expect_throttling=*/false);

  // last_update record > max_throttle_time : should never throttle.
  TestThrottling(base::Time::Now() - kWebViewAppsMaxAllowlistThrottleTimeDelta -
                     base::Days(1),
                 &client,
                 /*expect_throttling=*/false);

  // min_throttle_time < last_update record < max_throttle_time : maybe
  // throttle.
  TestThrottling(base::Time::Now() - kWebViewAppsMaxAllowlistThrottleTimeDelta +
                     kWebViewAppsMinAllowlistThrottleTimeDelta,
                 &client,
                 /*expect_throttling=*/expect_throttling);

  // last_update record < min_throttle_time : should always throttle.
  TestThrottling(base::Time::Now() - kWebViewAppsMinAllowlistThrottleTimeDelta +
                     base::Minutes(30),
                 &client,
                 /*expect_throttling=*/true);
}

}  // namespace

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestThrottlingAllowlist_AbsentCache) {
  base::SetRecordActionTaskRunner(env_.GetMainThreadTaskRunner());

  TestThrottlingAllowlist(absl::optional<AppPackageNameLoggingRule>(),
                          /*expect_throttling=*/false);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestThrottlingAllowlist_ValidCacheAllowedApp) {
  base::SetRecordActionTaskRunner(env_.GetMainThreadTaskRunner());

  TestThrottlingAllowlist(
      AppPackageNameLoggingRule(base::Version(kTestAllowlistVersion),
                                base::Time::Now() + base::Days(1)),
      /*expect_throttling=*/true);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestThrottlingAllowlist_ValidCacheNotAllowedApp) {
  base::SetRecordActionTaskRunner(env_.GetMainThreadTaskRunner());

  TestThrottlingAllowlist(
      AppPackageNameLoggingRule(base::Version(kTestAllowlistVersion),
                                base::Time::Min()),
      /*expect_throttling=*/true);
}

TEST_F(AwAppsPackageNamesAllowlistComponentLoaderPolicyTest,
       TestThrottlingAllowlist_ExpiredAllowedCache) {
  base::SetRecordActionTaskRunner(env_.GetMainThreadTaskRunner());

  TestThrottlingAllowlist(
      AppPackageNameLoggingRule(base::Version(kTestAllowlistVersion),
                                base::Time::Now() - base::Days(1)),
      /*expect_throttling=*/false);
}

}  // namespace android_webview
