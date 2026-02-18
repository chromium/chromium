// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/android_metrics_provider.h"

#include "android_webview/browser/metrics/system_state_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace android_webview {

namespace {

class AndroidMetricsProviderTest : public testing::Test {
 public:
  AndroidMetricsProviderTest() : metrics_provider_(&pref_service_) {
    AndroidMetricsProvider::RegisterPrefs(pref_service_.registry());
  }
  ~AndroidMetricsProviderTest() override {
    AndroidMetricsProvider::ResetGlobalStateForTesting();
  }

 protected:
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
  AndroidMetricsProvider metrics_provider_;
  metrics::ChromeUserMetricsExtension uma_proto_;
};

}  // namespace

TEST_F(AndroidMetricsProviderTest, OnDidCreateMetricsLog) {
  metrics_provider_.OnDidCreateMetricsLog();
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester_.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
  histogram_tester_.ExpectTotalCount("Android.MultipleUserProfilesState", 1);
  histogram_tester_.ExpectTotalCount("Android.WebView.PrimaryCpuAbiBitness", 1);
  histogram_tester_.ExpectTotalCount("Android.WebView.AgsaProcessName", 0);
}

TEST_F(AndroidMetricsProviderTest, ProvidePreviousSessionData) {
  metrics_provider_.ProvidePreviousSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 0);
  histogram_tester_.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
  histogram_tester_.ExpectTotalCount("Android.MultipleUserProfilesState", 1);
  histogram_tester_.ExpectTotalCount("Android.WebView.PrimaryCpuAbiBitness", 0);
  histogram_tester_.ExpectTotalCount("Android.WebView.AgsaProcessName", 0);
}

TEST_F(AndroidMetricsProviderTest,
       ProvidePreviousSessionDataWithSavedLocalState) {
  metrics::AndroidMetricsHelper::SaveLocalState(&pref_service_, 588700002);
  pref_service_.SetInteger(prefs::kPrimaryCpuAbiBitnessPref, 1);
  metrics_provider_.ProvidePreviousSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester_.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
  histogram_tester_.ExpectTotalCount("Android.MultipleUserProfilesState", 1);
  histogram_tester_.ExpectTotalCount("Android.WebView.PrimaryCpuAbiBitness", 1);
  histogram_tester_.ExpectTotalCount("Android.WebView.AgsaProcessName", 0);
}

TEST_F(AndroidMetricsProviderTest, AgsaProcessNameMapping) {
  EXPECT_EQ(internal::GetAgsaProcessNameEnumImpl(
                "com.google.android.googlequicksearchbox:googleapp"),
            AgsaProcessName::kGoogleApp);
  EXPECT_EQ(internal::GetAgsaProcessNameEnumImpl(
                "com.google.android.googlequicksearchbox:search"),
            AgsaProcessName::kSearch);
  EXPECT_EQ(internal::GetAgsaProcessNameEnumImpl(
                "com.google.android.googlequicksearchbox:interactor"),
            AgsaProcessName::kInteractor);
  EXPECT_EQ(internal::GetAgsaProcessNameEnumImpl(
                "com.google.android.googlequicksearchbox"),
            AgsaProcessName::kOther);
  EXPECT_EQ(internal::GetAgsaProcessNameEnumImpl("other"),
            AgsaProcessName::kOther);
}

}  // namespace android_webview
