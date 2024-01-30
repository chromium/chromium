// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr BinaryUploadService::Result kAllBinaryUploadServiceResults[]{
    BinaryUploadService::Result::UNKNOWN,
    BinaryUploadService::Result::SUCCESS,
    BinaryUploadService::Result::UPLOAD_FAILURE,
    BinaryUploadService::Result::TIMEOUT,
    BinaryUploadService::Result::FILE_TOO_LARGE,
    BinaryUploadService::Result::FAILED_TO_GET_TOKEN,
    BinaryUploadService::Result::UNAUTHORIZED,
    BinaryUploadService::Result::FILE_ENCRYPTED,
};

#if !BUILDFLAG(USE_CRASH_KEY_STUBS)
constexpr std::pair<ScanningCrashKey, const char*> kAllCrashKeys[] = {
    {ScanningCrashKey::PENDING_FILE_UPLOADS, "pending-file-upload-scans"},
    {ScanningCrashKey::PENDING_FILE_DOWNLOADS, "pending-file-download-scans"},
    {ScanningCrashKey::PENDING_TEXT_UPLOADS, "pending-text-upload-scans"},
    {ScanningCrashKey::PENDING_PRINTS, "pending-print-scans"},
    {ScanningCrashKey::TOTAL_FILE_UPLOADS, "total-file-upload-scans"},
    {ScanningCrashKey::TOTAL_FILE_DOWNLOADS, "total-file-download-scans"},
    {ScanningCrashKey::TOTAL_TEXT_UPLOADS, "total-text-upload-scans"},
    {ScanningCrashKey::TOTAL_PRINTS, "total-print-scans"}};
#endif  // !BUILDFLAG(USE_CRASH_KEY_STUBS)

constexpr int64_t kTotalBytes = 1000;

constexpr base::TimeDelta kDuration = base::Seconds(10);

constexpr base::TimeDelta kInvalidDuration = base::Seconds(0);

}  // namespace

class DeepScanningUtilsUMATest
    : public testing::TestWithParam<
          std::tuple<bool, DeepScanAccessPoint, BinaryUploadService::Result>> {
 public:
  DeepScanningUtilsUMATest() {}

  bool is_cloud() const { return std::get<0>(GetParam()); }

  DeepScanAccessPoint access_point() const { return std::get<1>(GetParam()); }

  std::string access_point_string() const {
    return DeepScanAccessPointToString(access_point());
  }

  BinaryUploadService::Result result() const { return std::get<2>(GetParam()); }

  bool success() const {
    return result() == BinaryUploadService::Result::SUCCESS;
  }

  std::string result_value(bool success) const {
    return BinaryUploadServiceResultToString(result(), success);
  }

  const base::HistogramTester& histograms() const { return histograms_; }

  const char* metric_prefix() {
    return is_cloud() ? "SafeBrowsing.DeepScan."
                      : "SafeBrowsing.LocalDeepScan.";
  }

 private:
  base::HistogramTester histograms_;
};

INSTANTIATE_TEST_SUITE_P(
    Tests,
    DeepScanningUtilsUMATest,
    testing::Combine(testing::Bool(),
                     testing::Values(DeepScanAccessPoint::DOWNLOAD,
                                     DeepScanAccessPoint::UPLOAD,
                                     DeepScanAccessPoint::DRAG_AND_DROP,
                                     DeepScanAccessPoint::PASTE,
                                     DeepScanAccessPoint::PRINT,
                                     DeepScanAccessPoint::FILE_TRANSFER),
                     testing::ValuesIn(kAllBinaryUploadServiceResults)));

TEST_P(DeepScanningUtilsUMATest, SuccessfulScanVerdicts) {
  // Record metrics for the 5 successful scan possibilities:
  // - A default response
  // - A DLP response with SUCCESS
  // - A malware respopnse with MALWARE, UWS, CLEAN
  RecordDeepScanMetrics(is_cloud(), access_point(), kDuration, kTotalBytes,
                        result(),
                        enterprise_connectors::ContentAnalysisResponse());
  RecordDeepScanMetrics(
      is_cloud(), access_point(), kDuration, kTotalBytes, result(),
      SimpleContentAnalysisResponseForTesting(
          /*dlp_success*/ true,
          /*malware_success*/ std::nullopt, /*has_custom_rule_message*/ false));
  for (const std::string& verdict : {"malware", "uws", "safe"}) {
    enterprise_connectors::ContentAnalysisResponse response;
    auto* malware_result = response.add_results();
    malware_result->set_tag("malware");
    malware_result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    if (verdict != "safe") {
      auto* rule = malware_result->add_triggered_rules();
      rule->set_rule_name("malware");
      rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
    }

    RecordDeepScanMetrics(is_cloud(), access_point(), kDuration, kTotalBytes,
                          result(), response);
  }

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(0u, histograms().GetTotalCountsForPrefix(metric_prefix()).size());
  } else {
    // We expect at least 2 histograms (<access-point>.Duration and
    // <access-point>.<result>.Duration), but only expect a third histogram in
    // the success case (bytes/seconds). Each of these should have 5 records to
    // match each of the RecordDeepScanMetrics calls.
    uint64_t expected_histograms = success() ? 3u : 2u;
    auto recorded = histograms().GetTotalCountsForPrefix(metric_prefix());
    EXPECT_EQ(expected_histograms, recorded.size());
    if (success()) {
      EXPECT_EQ(recorded[metric_prefix() + access_point_string() +
                         ".BytesPerSeconds"],
                5);
    }
    EXPECT_EQ(recorded[metric_prefix() + access_point_string() + ".Duration"],
              5);
    EXPECT_EQ(recorded[metric_prefix() + access_point_string() + "." +
                       result_value(true) + ".Duration"],
              5);
  }
}

TEST_P(DeepScanningUtilsUMATest, UnsuccessfulDlpScanVerdicts) {
  // Record metrics for the 2 unsuccessful DLP scan possibilities.
  for (const auto status :
       {enterprise_connectors::ContentAnalysisResponse::Result::FAILURE,
        enterprise_connectors::ContentAnalysisResponse::Result::
            STATUS_UNKNOWN}) {
    enterprise_connectors::ContentAnalysisResponse response;
    auto* dlp_result = response.add_results();
    dlp_result->set_tag("dlp");
    dlp_result->set_status(status);

    RecordDeepScanMetrics(is_cloud(), access_point(), kDuration, kTotalBytes,
                          result(), response);
  }

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(0u, histograms().GetTotalCountsForPrefix(metric_prefix()).size());
  } else {
    auto recorded = histograms().GetTotalCountsForPrefix(metric_prefix());
    EXPECT_EQ(2u, recorded.size());
    EXPECT_EQ(recorded[metric_prefix() + access_point_string() + ".Duration"],
              2);
    EXPECT_EQ(recorded[metric_prefix() + access_point_string() + "." +
                       result_value(false) + ".Duration"],
              2);
  }
}

TEST_P(DeepScanningUtilsUMATest, UnsuccessfulMalwareScanVerdict) {
  // Record metrics for the 2 unsuccessful malware scan possibilities.
  for (const auto status :
       {enterprise_connectors::ContentAnalysisResponse::Result::FAILURE,
        enterprise_connectors::ContentAnalysisResponse::Result::
            STATUS_UNKNOWN}) {
    enterprise_connectors::ContentAnalysisResponse response;
    auto* malware_result = response.add_results();
    malware_result->set_tag("malware");
    malware_result->set_status(status);

    RecordDeepScanMetrics(is_cloud(), access_point(), kDuration, kTotalBytes,
                          result(), response);
  }

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(0u, histograms().GetTotalCountsForPrefix(metric_prefix()).size());
  } else {
    auto recorded = histograms().GetTotalCountsForPrefix(metric_prefix());
    EXPECT_EQ(2u, recorded.size());
    EXPECT_EQ(recorded[metric_prefix() + access_point_string() + ".Duration"],
              2);
    EXPECT_EQ(recorded[metric_prefix() + access_point_string() + "." +
                       result_value(false) + ".Duration"],
              2);
  }
}

TEST_P(DeepScanningUtilsUMATest, BypassScanVerdict) {
  RecordDeepScanMetrics(is_cloud(), access_point(), kDuration, kTotalBytes,
                        "BypassedByUser", false);

  EXPECT_EQ(2u, histograms().GetTotalCountsForPrefix(metric_prefix()).size());
  histograms().ExpectTimeBucketCount(
      metric_prefix() + access_point_string() + ".Duration", kDuration, 1);
  histograms().ExpectTimeBucketCount(
      metric_prefix() + access_point_string() + ".BypassedByUser.Duration",
      kDuration, 1);
}

TEST_P(DeepScanningUtilsUMATest, CancelledByUser) {
  RecordDeepScanMetrics(is_cloud(), access_point(), kDuration, kTotalBytes,
                        "CancelledByUser", false);

  EXPECT_EQ(2u, histograms().GetTotalCountsForPrefix(metric_prefix()).size());
  histograms().ExpectTimeBucketCount(
      metric_prefix() + access_point_string() + ".Duration", kDuration, 1);
  histograms().ExpectTimeBucketCount(
      metric_prefix() + access_point_string() + ".CancelledByUser.Duration",
      kDuration, 1);
}

TEST_P(DeepScanningUtilsUMATest, InvalidDuration) {
  RecordDeepScanMetrics(is_cloud(), access_point(), kInvalidDuration,
                        kTotalBytes, result(),
                        enterprise_connectors::ContentAnalysisResponse());
  EXPECT_EQ(0u, histograms().GetTotalCountsForPrefix(metric_prefix()).size());
}

#if !BUILDFLAG(USE_CRASH_KEY_STUBS)
class DeepScanningUtilsCrashKeysTest
    : public testing::TestWithParam<std::pair<ScanningCrashKey, const char*>> {
 public:
  void SetUp() override {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeysForTesting();
  }

  void TearDown() override { crash_reporter::ResetCrashKeysForTesting(); }

  ScanningCrashKey key_enum() { return std::get<0>(GetParam()); }

  const char* key_string() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         DeepScanningUtilsCrashKeysTest,
                         testing::ValuesIn(kAllCrashKeys));

TEST_P(DeepScanningUtilsCrashKeysTest, SmallModifications) {
  // The key implicitly starts at 0.
  IncrementCrashKey(key_enum(), 1);
  EXPECT_EQ("1", crash_reporter::GetCrashKeyValue(key_string()));

  IncrementCrashKey(key_enum(), 1);
  EXPECT_EQ("2", crash_reporter::GetCrashKeyValue(key_string()));

  DecrementCrashKey(key_enum(), 1);
  EXPECT_EQ("1", crash_reporter::GetCrashKeyValue(key_string()));

  DecrementCrashKey(key_enum(), 1);
  EXPECT_TRUE(crash_reporter::GetCrashKeyValue(key_string()).empty());
}

TEST_P(DeepScanningUtilsCrashKeysTest, LargeModifications) {
  // The key implicitly starts at 0.
  IncrementCrashKey(key_enum(), 100);
  EXPECT_EQ("100", crash_reporter::GetCrashKeyValue(key_string()));

  IncrementCrashKey(key_enum(), 100);
  EXPECT_EQ("200", crash_reporter::GetCrashKeyValue(key_string()));

  DecrementCrashKey(key_enum(), 100);
  EXPECT_EQ("100", crash_reporter::GetCrashKeyValue(key_string()));

  DecrementCrashKey(key_enum(), 100);
  EXPECT_TRUE(crash_reporter::GetCrashKeyValue(key_string()).empty());
}

TEST_P(DeepScanningUtilsCrashKeysTest, InvalidModifications) {
  // The crash key value cannot be negative.
  DecrementCrashKey(key_enum(), 1);
  EXPECT_TRUE(crash_reporter::GetCrashKeyValue(key_string()).empty());
  DecrementCrashKey(key_enum(), 100);
  EXPECT_TRUE(crash_reporter::GetCrashKeyValue(key_string()).empty());

  // The crash key value is restricted to 6 digits. If a modification would
  // exceed it, it is clamped so crashes will indicate that the key was set at a
  // very high value.
  IncrementCrashKey(key_enum(), 123456789);
  EXPECT_EQ("999999", crash_reporter::GetCrashKeyValue(key_string()));
  IncrementCrashKey(key_enum(), 123456789);
  EXPECT_EQ("999999", crash_reporter::GetCrashKeyValue(key_string()));
}
#endif  // !BUILDFLAG(USE_CRASH_KEY_STUBS)

}  // namespace safe_browsing
