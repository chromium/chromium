// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/android_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace android_webview {

namespace {

class AndroidMetricsProviderTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
  AndroidMetricsProvider metrics_provider_;
  metrics::ChromeUserMetricsExtension uma_proto_;
};

}  // namespace

TEST_F(AndroidMetricsProviderTest, ProvideCurrentSessionData) {
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester_.ExpectTotalCount("Android.AbiBitnessSupport", 1);
}

TEST_F(AndroidMetricsProviderTest, ProvidePreviousSessionData) {
  metrics_provider_.ProvidePreviousSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 0);
  histogram_tester_.ExpectTotalCount("Android.AbiBitnessSupport", 1);
}

}  // namespace android_webview
