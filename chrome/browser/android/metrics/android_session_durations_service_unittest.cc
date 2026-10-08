// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_session_durations_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kResumeMetricName[] =
    "Profile.Incognito.ResumedAfterReportedDuration";
constexpr char kBackgroundMetricName[] =
    "Profile.Incognito.MovedToBackgroundAfterDuration";
constexpr char kIsolatedResumeMetricName[] =
    "Profile.Isolated.ResumedAfterReportedDuration";
constexpr char kIsolatedBackgroundMetricName[] =
    "Profile.Isolated.MovedToBackgroundAfterDuration";

using OffTheRecordProfileType =
    AndroidSessionDurationsService::OffTheRecordProfileType;

}  // namespace

class AndroidIncognitoSessionDurationsServiceTest
    : public testing::TestWithParam<OffTheRecordProfileType> {
 public:
  AndroidIncognitoSessionDurationsServiceTest() = default;

  AndroidIncognitoSessionDurationsServiceTest(
      const AndroidIncognitoSessionDurationsServiceTest&) = delete;
  AndroidIncognitoSessionDurationsServiceTest& operator=(
      const AndroidIncognitoSessionDurationsServiceTest&) = delete;

  ~AndroidIncognitoSessionDurationsServiceTest() override = default;

  const char* GetResumeMetricName() const {
    return GetParam() == OffTheRecordProfileType::kIsolated
               ? kIsolatedResumeMetricName
               : kResumeMetricName;
  }

  const char* GetBackgroundMetricName() const {
    return GetParam() == OffTheRecordProfileType::kIsolated
               ? kIsolatedBackgroundMetricName
               : kBackgroundMetricName;
  }

  const char* GetOtherResumeMetricName() const {
    return GetParam() == OffTheRecordProfileType::kIsolated
               ? kResumeMetricName
               : kIsolatedResumeMetricName;
  }

  const char* GetOtherBackgroundMetricName() const {
    return GetParam() == OffTheRecordProfileType::kIsolated
               ? kBackgroundMetricName
               : kIsolatedBackgroundMetricName;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AndroidIncognitoSessionDurationsServiceTest,
                         testing::Values(OffTheRecordProfileType::kIncognito,
                                         OffTheRecordProfileType::kIsolated));

TEST_P(AndroidIncognitoSessionDurationsServiceTest, RegularIncognitoClose) {
  base::HistogramTester histograms;

  {
    // Start service.
    auto service = std::make_unique<AndroidSessionDurationsService>();
    service->InitializeForIncognitoAndIsolatedProfile(GetParam());

    histograms.ExpectTotalCount(GetResumeMetricName(), 0);
    histograms.ExpectTotalCount(GetBackgroundMetricName(), 0);

    // Close service (happens when Incognito profile is properly closed).
    service->Shutdown();
  }

  // Check after service shutdown and destruction.
  histograms.ExpectTotalCount(GetResumeMetricName(), 0);
  histograms.ExpectBucketCount(GetBackgroundMetricName(), 0, 1);
  histograms.ExpectTotalCount(GetOtherResumeMetricName(), 0);
  histograms.ExpectTotalCount(GetOtherBackgroundMetricName(), 0);
}

TEST_P(AndroidIncognitoSessionDurationsServiceTest, DieInBackground) {
  base::HistogramTester histograms;

  {
    // Start service.
    auto service = std::make_unique<AndroidSessionDurationsService>();
    service->InitializeForIncognitoAndIsolatedProfile(GetParam());

    histograms.ExpectTotalCount(GetResumeMetricName(), 0);
    histograms.ExpectTotalCount(GetBackgroundMetricName(), 0);

    // Go background.
    service->OnAppEnterBackground(base::TimeDelta());
    histograms.ExpectTotalCount(GetResumeMetricName(), 0);
    histograms.ExpectBucketCount(GetBackgroundMetricName(), 0, 1);
  }

  // Check again after service destruction.
  histograms.ExpectTotalCount(GetResumeMetricName(), 0);
  histograms.ExpectTotalCount(GetBackgroundMetricName(), 1);
  histograms.ExpectTotalCount(GetOtherResumeMetricName(), 0);
  histograms.ExpectTotalCount(GetOtherBackgroundMetricName(), 0);
}

TEST_P(AndroidIncognitoSessionDurationsServiceTest, DoubleForeground) {
  base::HistogramTester histograms;

  // Start service and move to foreground and expect no recording.
  auto service = std::make_unique<AndroidSessionDurationsService>();
  service->InitializeForIncognitoAndIsolatedProfile(GetParam());

  service->OnAppEnterForeground(base::TimeTicks());
  histograms.ExpectTotalCount(GetResumeMetricName(), 0);
  histograms.ExpectTotalCount(GetBackgroundMetricName(), 0);
  histograms.ExpectTotalCount(GetOtherResumeMetricName(), 0);
  histograms.ExpectTotalCount(GetOtherBackgroundMetricName(), 0);
}

TEST_P(AndroidIncognitoSessionDurationsServiceTest, MultipleStateChange) {
  base::HistogramTester histograms;

  auto service = std::make_unique<AndroidSessionDurationsService>();
  service->InitializeForIncognitoAndIsolatedProfile(GetParam());

  // Go background.
  service->OnAppEnterBackground(base::TimeDelta());
  histograms.ExpectTotalCount(GetResumeMetricName(), 0);
  histograms.ExpectBucketCount(GetBackgroundMetricName(), 0, 1);

  // Go foreground.
  service->OnAppEnterForeground(base::TimeTicks());
  histograms.ExpectBucketCount(GetResumeMetricName(), 0, 1);

  // Assume session start was 1 hour ago and go background.
  service->SetSessionStartTimeForTesting(base::Time::Now() - base::Hours(1));
  service->OnAppEnterBackground(base::TimeDelta());
  histograms.ExpectBucketCount(GetBackgroundMetricName(), 60, 1);

  // Go foreground.
  service->OnAppEnterForeground(base::TimeTicks());
  histograms.ExpectBucketCount(GetResumeMetricName(), 60, 1);
  histograms.ExpectTotalCount(GetBackgroundMetricName(), 2);

  histograms.ExpectTotalCount(GetOtherResumeMetricName(), 0);
  histograms.ExpectTotalCount(GetOtherBackgroundMetricName(), 0);
}
