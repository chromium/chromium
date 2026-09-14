// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_entropy_state_provider.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {
namespace {

class AwEntropyStateProviderTest : public testing::Test {
 public:
  AwEntropyStateProviderTest() {
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    AwEntropyStateProvider::RegisterPrefs(prefs_.registry());
  }
  ~AwEntropyStateProviderTest() override = default;

  AwEntropyStateProviderTest(const AwEntropyStateProviderTest&) = delete;
  AwEntropyStateProviderTest& operator=(const AwEntropyStateProviderTest&) =
      delete;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(AwEntropyStateProviderTest,
       PopulateStandardAndWebViewLowEntropySources) {
  const int new_low_source = 1234;
  const int old_low_source = 5678;
  const int pseudo_low_source = 4321;
  const int webview_low_source = 7049;

  prefs_.SetInteger(metrics::prefs::kMetricsLowEntropySource, new_low_source);
  prefs_.SetInteger(metrics::prefs::kMetricsOldLowEntropySource,
                    old_low_source);
  prefs_.SetInteger(metrics::prefs::kMetricsPseudoLowEntropySource,
                    pseudo_low_source);
  AwEntropyStateProvider provider(&prefs_);
  prefs_.SetInteger(prefs::kWebViewLowEntropySource, webview_low_source);

  metrics::SystemProfileProto system_profile;

  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_EQ(new_low_source, system_profile.low_entropy_source());
  EXPECT_EQ(old_low_source, system_profile.old_low_entropy_source());
  EXPECT_EQ(pseudo_low_source, system_profile.pseudo_low_entropy_source());
  EXPECT_TRUE(system_profile.has_webview_low_entropy_source());
  EXPECT_EQ(webview_low_source, system_profile.webview_low_entropy_source());
}

TEST_F(AwEntropyStateProviderTest, ProvideSystemProfileMetrics_ValueUnset) {
  AwEntropyStateProvider provider(&prefs_);
  metrics::SystemProfileProto system_profile;

  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_FALSE(system_profile.has_webview_low_entropy_source());
}

TEST_F(AwEntropyStateProviderTest,
       ProvideSystemProfileMetrics_NegativeValueIgnored) {
  AwEntropyStateProvider provider(&prefs_);
  prefs_.SetInteger(prefs::kWebViewLowEntropySource, -1);
  metrics::SystemProfileProto system_profile;

  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_FALSE(system_profile.has_webview_low_entropy_source());
}

}  // namespace
}  // namespace android_webview
