// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_session_durations_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char resume_metric_name[] =
    "Profile.Incognito.ResumedAfterReportedDuration";
const char background_metric_name[] =
    "Profile.Incognito.MovedToBackgroundAfterDuration";

}  // namespace

class AndroidIncognitoSessionDurationsServiceTest : public testing::Test {
 public:
  AndroidIncognitoSessionDurationsServiceTest() = default;

  AndroidIncognitoSessionDurationsServiceTest(
      const AndroidIncognitoSessionDurationsServiceTest&) = delete;
  AndroidIncognitoSessionDurationsServiceTest& operator=(
      const AndroidIncognitoSessionDurationsServiceTest&) = delete;

  ~AndroidIncognitoSessionDurationsServiceTest() override = default;
};

TEST_F(AndroidIncognitoSessionDurationsServiceTest, RegularIncognitoClose) {
  base::HistogramTester histograms;

  {
    // Start service.
    auto service = std::make_unique<AndroidSessionDurationsService>();
    service->InitializeForIncognitoProfile();

    histograms.ExpectTotalCount(resume_metric_name, 0);
    histograms.ExpectTotalCount(background_metric_name, 0);

    // Close service (happens when Incognito profile is properly closed).
    service->Shutdown();
  }

  // Check after service shutdown and destruction.
  histograms.ExpectTotalCount(resume_metric_name, 0);
  histograms.ExpectBucketCount(background_metric_name, 0, 1);
}

TEST_F(AndroidIncognitoSessionDurationsServiceTest, DieInBackground) {
  base::HistogramTester histograms;

  {
    // Start service.
    auto service = std::make_unique<AndroidSessionDurationsService>();
    service->InitializeForIncognitoProfile();

    histograms.ExpectTotalCount(resume_metric_name, 0);
    histograms.ExpectTotalCount(background_metric_name, 0);

    // Go background.
    service->OnAppEnterBackground(base::TimeDelta());
    histograms.ExpectTotalCount(resume_metric_name, 0);
    histograms.ExpectBucketCount(background_metric_name, 0, 1);
  }

  // Check again after service destruction.
  histograms.ExpectTotalCount(resume_metric_name, 0);
  histograms.ExpectTotalCount(background_metric_name, 1);
}

TEST_F(AndroidIncognitoSessionDurationsServiceTest, DoubleForeground) {
  base::HistogramTester histograms;

  // Start service and move to foreground and expect no recording.
  auto service = std::make_unique<AndroidSessionDurationsService>();
  service->InitializeForIncognitoProfile();

  service->OnAppEnterForeground(base::TimeTicks());
  histograms.ExpectTotalCount(resume_metric_name, 0);
  histograms.ExpectTotalCount(background_metric_name, 0);
}

TEST_F(AndroidIncognitoSessionDurationsServiceTest, MultipleStateChange) {
  base::HistogramTester histograms;

  auto service = std::make_unique<AndroidSessionDurationsService>();
  service->InitializeForIncognitoProfile();

  // Go background.
  service->OnAppEnterBackground(base::TimeDelta());
  histograms.ExpectTotalCount(resume_metric_name, 0);
  histograms.ExpectBucketCount(background_metric_name, 0, 1);

  // Go foreground.
  service->OnAppEnterForeground(base::TimeTicks());
  histograms.ExpectBucketCount(resume_metric_name, 0, 1);

  // Assume session start was 1 hour ago and go background.
  service->SetSessionStartTimeForTesting(base::Time::Now() -
                                         base::Seconds(60) * 60);
  service->OnAppEnterBackground(base::TimeDelta());
  histograms.ExpectBucketCount(background_metric_name, 60, 1);

  // Go foreground.
  service->OnAppEnterForeground(base::TimeTicks());
  histograms.ExpectBucketCount(resume_metric_name, 60, 1);
  histograms.ExpectTotalCount(background_metric_name, 2);
}
