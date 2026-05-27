// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/android_metrics_log_uploader.h"

#include "base/metrics/metrics_hashes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

namespace {

// Some UMA histogram names from the allowlist.
const char kAllowedHistogram1[] = "Android.WebView.Visibility.Global";
const char kAllowedHistogram2[] = "Android.WebView.SafeMode.SafeModeEnabled";

// Some UMA histogram names NOT in the allowlist.
const char kDisallowedHistogram1[] = "Histogram.Not.In.Allowlist";
const char kDisallowedHistogram2[] = "Another.Disallowed.Histogram";

}  // namespace

TEST(AndroidMetricsLogUploaderTest, MaybeFilterLog_NoFiltering) {
  ChromeUserMetricsExtension proto;
  // Set status to METRICS_ALL (no filtering).
  proto.mutable_system_profile()->set_metrics_filtering_status(
      SystemProfileProto::METRICS_ALL);

  // Add some histograms.
  proto.add_histogram_event()->set_name_hash(
      base::HashMetricName(kAllowedHistogram1));
  proto.add_histogram_event()->set_name_hash(
      base::HashMetricName(kDisallowedHistogram1));

  // Add some user actions.
  proto.add_user_action_event()->set_name_hash(12345);

  std::string log_data;
  proto.SerializeToString(&log_data);

  // Run filter.
  AndroidMetricsLogUploader::MaybeFilterLog(log_data);

  // Verify proto is unchanged.
  ChromeUserMetricsExtension parsed_proto;
  ASSERT_TRUE(parsed_proto.ParseFromString(log_data));
  EXPECT_EQ(2, parsed_proto.histogram_event_size());
  EXPECT_EQ(1, parsed_proto.user_action_event_size());
}

TEST(AndroidMetricsLogUploaderTest, MaybeFilterLog_ApplyFiltering) {
  ChromeUserMetricsExtension proto;
  // Set status to METRICS_ONLY_CRITICAL.
  proto.mutable_system_profile()->set_metrics_filtering_status(
      SystemProfileProto::METRICS_ONLY_CRITICAL);

  // Add some histograms.
  proto.add_histogram_event()->set_name_hash(
      base::HashMetricName(kAllowedHistogram1));
  proto.add_histogram_event()->set_name_hash(
      base::HashMetricName(kDisallowedHistogram1));
  proto.add_histogram_event()->set_name_hash(
      base::HashMetricName(kAllowedHistogram2));
  proto.add_histogram_event()->set_name_hash(
      base::HashMetricName(kDisallowedHistogram2));

  // Add some user actions.
  proto.add_user_action_event()->set_name_hash(12345);

  std::string log_data;
  proto.SerializeToString(&log_data);

  // Run filter.
  AndroidMetricsLogUploader::MaybeFilterLog(log_data);

  // Verify proto is filtered.
  ChromeUserMetricsExtension parsed_proto;
  ASSERT_TRUE(parsed_proto.ParseFromString(log_data));

  // User actions should be cleared.
  EXPECT_EQ(0, parsed_proto.user_action_event_size());

  // Only allowed histograms should remain.
  ASSERT_EQ(2, parsed_proto.histogram_event_size());
  EXPECT_EQ(base::HashMetricName(kAllowedHistogram1),
            parsed_proto.histogram_event(0).name_hash());
  EXPECT_EQ(base::HashMetricName(kAllowedHistogram2),
            parsed_proto.histogram_event(1).name_hash());
}

TEST(AndroidMetricsLogUploaderTest, MaybeFilterLog_InvalidProto) {
  std::string log_data = "invalid proto data";
  std::string original_log_data = log_data;
  AndroidMetricsLogUploader::MaybeFilterLog(log_data);
  EXPECT_EQ(original_log_data, log_data);
}

}  // namespace metrics
