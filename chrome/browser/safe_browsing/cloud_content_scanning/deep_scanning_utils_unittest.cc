// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/common.h"
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
    BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE,
};

constexpr int64_t kTotalBytes = 1000;

constexpr base::TimeDelta kDuration = base::TimeDelta::FromSeconds(10);

constexpr base::TimeDelta kInvalidDuration = base::TimeDelta::FromSeconds(0);

}  // namespace

class DeepScanningUtilsUMATest
    : public testing::TestWithParam<
          std::tuple<DeepScanAccessPoint, BinaryUploadService::Result>> {
 public:
  DeepScanningUtilsUMATest() {}

  DeepScanAccessPoint access_point() const { return std::get<0>(GetParam()); }

  std::string access_point_string() const {
    return DeepScanAccessPointToString(access_point());
  }

  BinaryUploadService::Result result() const { return std::get<1>(GetParam()); }

  bool success() const {
    return result() == BinaryUploadService::Result::SUCCESS;
  }

  std::string result_value(bool success) const {
    return BinaryUploadServiceResultToString(result(), success);
  }

  const base::HistogramTester& histograms() const { return histograms_; }

 private:
  base::HistogramTester histograms_;
};

INSTANTIATE_TEST_SUITE_P(
    Tests,
    DeepScanningUtilsUMATest,
    testing::Combine(testing::Values(DeepScanAccessPoint::DOWNLOAD,
                                     DeepScanAccessPoint::UPLOAD,
                                     DeepScanAccessPoint::DRAG_AND_DROP,
                                     DeepScanAccessPoint::PASTE),
                     testing::ValuesIn(kAllBinaryUploadServiceResults)));

TEST_P(DeepScanningUtilsUMATest, SuccessfulScanVerdicts) {
  // Record metrics for the 5 successful scan possibilities:
  // - A default response
  // - A DLP response with SUCCESS
  // - A malware respopnse with MALWARE, UWS, CLEAN
  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                        enterprise_connectors::ContentAnalysisResponse());
  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                        SimpleContentAnalysisResponseForTesting(
                            /*dlp_success*/ true,
                            /*malware_success*/ base::nullopt));
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

    RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                          response);
  }

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(
        0u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  } else {
    // We expect at least 2 histograms (<access-point>.Duration and
    // <access-point>.<result>.Duration), but only expect a third histogram in
    // the success case (bytes/seconds). Each of these should have 5 records to
    // match each of the RecordDeepScanMetrics calls.
    uint64_t expected_histograms = success() ? 3u : 2u;
    auto recorded =
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.");
    EXPECT_EQ(expected_histograms, recorded.size());
    if (success()) {
      EXPECT_EQ(recorded["SafeBrowsing.DeepScan." + access_point_string() +
                         ".BytesPerSeconds"],
                5);
    }
    EXPECT_EQ(recorded["SafeBrowsing.DeepScan." + access_point_string() +
                       ".Duration"],
              5);
    EXPECT_EQ(recorded["SafeBrowsing.DeepScan." + access_point_string() + "." +
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

    RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                          response);
  }

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(
        0u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  } else {
    auto recorded =
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.");
    EXPECT_EQ(2u, recorded.size());
    EXPECT_EQ(recorded["SafeBrowsing.DeepScan." + access_point_string() +
                       ".Duration"],
              2);
    EXPECT_EQ(recorded["SafeBrowsing.DeepScan." + access_point_string() + "." +
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

    RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes, result(),
                          response);
  }

  if (result() == BinaryUploadService::Result::UNAUTHORIZED) {
    EXPECT_EQ(
        0u,
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  } else {
    auto recorded =
        histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.");
    EXPECT_EQ(2u, recorded.size());
    EXPECT_EQ(recorded["SafeBrowsing.DeepScan." + access_point_string() +
                       ".Duration"],
              2);
    EXPECT_EQ(recorded["SafeBrowsing.DeepScan." + access_point_string() + "." +
                       result_value(false) + ".Duration"],
              2);
  }
}

TEST_P(DeepScanningUtilsUMATest, BypassScanVerdict) {
  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes,
                        "BypassedByUser", false);

  EXPECT_EQ(
      2u,
      histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  histograms().ExpectTimeBucketCount(
      "SafeBrowsing.DeepScan." + access_point_string() + ".Duration", kDuration,
      1);
  histograms().ExpectTimeBucketCount("SafeBrowsing.DeepScan." +
                                         access_point_string() +
                                         ".BypassedByUser.Duration",
                                     kDuration, 1);
}

TEST_P(DeepScanningUtilsUMATest, CancelledByUser) {
  RecordDeepScanMetrics(access_point(), kDuration, kTotalBytes,
                        "CancelledByUser", false);

  EXPECT_EQ(
      2u,
      histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
  histograms().ExpectTimeBucketCount(
      "SafeBrowsing.DeepScan." + access_point_string() + ".Duration", kDuration,
      1);
  histograms().ExpectTimeBucketCount("SafeBrowsing.DeepScan." +
                                         access_point_string() +
                                         ".CancelledByUser.Duration",
                                     kDuration, 1);
}

TEST_P(DeepScanningUtilsUMATest, InvalidDuration) {
  RecordDeepScanMetrics(access_point(), kInvalidDuration, kTotalBytes, result(),
                        enterprise_connectors::ContentAnalysisResponse());
  EXPECT_EQ(
      0u,
      histograms().GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
}

class DeepScanningUtilsFileTypeSupportedTest : public testing::Test {
 protected:
  std::vector<base::FilePath::StringType> UnsupportedDlpFileTypes() {
    return {FILE_PATH_LITERAL(".these"), FILE_PATH_LITERAL(".types"),
            FILE_PATH_LITERAL(".are"), FILE_PATH_LITERAL(".not"),
            FILE_PATH_LITERAL(".supported")};
  }

  base::FilePath FilePath(const base::FilePath::StringType& type) {
    return base::FilePath(FILE_PATH_LITERAL("foo") + type);
  }
};

TEST_F(DeepScanningUtilsFileTypeSupportedTest, DLP) {
  // With a DLP-only scan, only the types returned by SupportedDlpFileTypes()
  // will be supported, and other types will fail.
  for (const base::FilePath::StringType& type : SupportedDlpFileTypes()) {
    EXPECT_TRUE(FileTypeSupportedForDlp(FilePath(type)));
  }
  for (const base::FilePath::StringType& type : UnsupportedDlpFileTypes()) {
    EXPECT_FALSE(FileTypeSupportedForDlp(FilePath(type)));
  }
}

}  // namespace safe_browsing
