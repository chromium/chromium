// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/error_screens_histogram_helper.h"

#include <memory>

#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kErrorShownSigninHistogram[] = "OOBE.NetworkErrorShown.Signin";
constexpr char kErrorTimeSigninPortalHistogram[] =
    "OOBE.ErrorScreensTime.Signin.Portal";
constexpr char kErrorTimeSigninNoneHistogram[] =
    "OOBE.ErrorScreensTime.Signin.None";

}  // namespace

class ErrorScreensHistogramHelperTest : public testing::Test {
 protected:
  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  base::HistogramTester histograms_;
  // The implementation is same for different `parent_screen`s so it is
  // sufficient to test a single one.
  std::unique_ptr<ErrorScreensHistogramHelper> signin_helper;

 private:
  void SetUp() override {
    signin_helper = std::make_unique<ErrorScreensHistogramHelper>(
        ErrorScreensHistogramHelper::ErrorParentScreen::kSignin);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// No errors when screen was not shown.
TEST_F(ErrorScreensHistogramHelperTest, DoesNotShowScreen) {
  signin_helper.reset();
  histograms_.ExpectTotalCount(kErrorShownSigninHistogram, 0);
}

// No errors when screen was shown and error was not.
TEST_F(ErrorScreensHistogramHelperTest, ShowScreenWithoutError) {
  signin_helper->OnScreenShow();
  signin_helper.reset();
  histograms_.ExpectUniqueSample(kErrorShownSigninHistogram,
                                 NetworkError::ERROR_STATE_NONE, 0);
}

// Show 3 offline errors and 1 portal error. Make sure in time histograms logged
// portal error only.
TEST_F(ErrorScreensHistogramHelperTest, ShowDifferentErrorsWithoutResolution) {
  signin_helper->OnScreenShow();
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  signin_helper->OnErrorHide();
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  FastForwardTime(base::Milliseconds(1000));
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_PORTAL);
  signin_helper.reset();
  histograms_.ExpectBucketCount(kErrorShownSigninHistogram,
                                NetworkError::ERROR_STATE_OFFLINE, 2);
  histograms_.ExpectBucketCount(kErrorShownSigninHistogram,
                                NetworkError::ERROR_STATE_PORTAL, 1);
  histograms_.ExpectUniqueTimeSample(kErrorTimeSigninPortalHistogram,
                                     base::Milliseconds(1000), 1);
}

// Show error and hide it after 1 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowHideTime) {
  signin_helper->OnScreenShow();
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  FastForwardTime(base::Milliseconds(1000));
  signin_helper->OnErrorHide();
  signin_helper.reset();
  histograms_.ExpectUniqueTimeSample(kErrorTimeSigninNoneHistogram,
                                     base::Milliseconds(1000), 1);
  histograms_.ExpectUniqueSample(kErrorShownSigninHistogram,
                                 NetworkError::ERROR_STATE_OFFLINE, 1);
}

// Show, hide, show, hide error with 1 sec interval. Make sure time logged in
// histogram is 2 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowHideShowHideTime) {
  signin_helper->OnScreenShow();

  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_PORTAL);
  FastForwardTime(base::Milliseconds(1000));
  signin_helper->OnErrorHide();

  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_PORTAL);
  FastForwardTime(base::Milliseconds(1000));
  signin_helper->OnErrorHide();

  FastForwardTime(base::Milliseconds(1000));
  signin_helper.reset();
  histograms_.ExpectUniqueSample(kErrorTimeSigninNoneHistogram, 2000, 1);
  histograms_.ExpectUniqueSample(kErrorShownSigninHistogram,
                                 NetworkError::ERROR_STATE_PORTAL, 2);
}

// Show, show, hide error with 1 sec interval. Make sure time logged in
// histogram is 2 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowShowHideTime) {
  signin_helper->OnScreenShow();
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_PROXY);
  FastForwardTime(base::Milliseconds(1000));
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_PORTAL);
  FastForwardTime(base::Milliseconds(1000));
  signin_helper->OnErrorHide();
  signin_helper.reset();
  histograms_.ExpectUniqueTimeSample(kErrorTimeSigninNoneHistogram,
                                     base::Milliseconds(2000), 1);
  histograms_.ExpectBucketCount(kErrorShownSigninHistogram,
                                NetworkError::ERROR_STATE_PROXY, 1);
  histograms_.ExpectBucketCount(kErrorShownSigninHistogram,
                                NetworkError::ERROR_STATE_PORTAL, 1);
}

// Check that calling OnErrorShow with the same error doesn't change total
// count.
TEST_F(ErrorScreensHistogramHelperTest, TestShowCalledSeveralTimes) {
  signin_helper->OnScreenShow();
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  FastForwardTime(base::Milliseconds(100));
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  signin_helper.reset();
  histograms_.ExpectUniqueSample(kErrorShownSigninHistogram,
                                 NetworkError::ERROR_STATE_OFFLINE, 1);
}

}  // namespace ash
