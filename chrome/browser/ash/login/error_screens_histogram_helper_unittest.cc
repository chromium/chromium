// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/error_screens_histogram_helper.h"

#include <memory>

#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kErrorShownEnrollmentHistogram[] =
    "OOBE.NetworkErrorShown.Enrollment";
constexpr char kErrorShownSigninHistogram[] = "OOBE.NetworkErrorShown.Signin";
constexpr char kErrorTimeEnrollmentPortalHistogram[] =
    "OOBE.ErrorScreensTime.Enrollment.Portal";
constexpr char kErrorTimeUserCreationProxyHistogram[] =
    "OOBE.ErrorScreensTime.UserCreation.Proxy";

}  // namespace

class ErrorScreensHistogramHelperTest : public testing::Test {
 public:
  void SetUp() override {
    enrollment_helper = std::make_unique<ErrorScreensHistogramHelper>(
        ErrorScreensHistogramHelper::ErrorParentScreen::kEnrollment);
    signin_helper = std::make_unique<ErrorScreensHistogramHelper>(
        ErrorScreensHistogramHelper::ErrorParentScreen::kSignin);
    user_creation_helper = std::make_unique<ErrorScreensHistogramHelper>(
        ErrorScreensHistogramHelper::ErrorParentScreen::kUserCreation);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histograms_;
  std::unique_ptr<ErrorScreensHistogramHelper> enrollment_helper;
  std::unique_ptr<ErrorScreensHistogramHelper> signin_helper;
  std::unique_ptr<ErrorScreensHistogramHelper> user_creation_helper;
};

// No errors when screen was not shown.
TEST_F(ErrorScreensHistogramHelperTest, DoesNotShowScreen) {
  enrollment_helper.reset();
  histograms_.ExpectTotalCount(kErrorShownEnrollmentHistogram, 0);
}

// No errors when screen was shown and error was not.
TEST_F(ErrorScreensHistogramHelperTest, ShowScreenWithoutError) {
  enrollment_helper->OnScreenShow();
  enrollment_helper.reset();
  signin_helper->OnScreenShow();
  signin_helper.reset();
  histograms_.ExpectUniqueSample(kErrorShownEnrollmentHistogram,
                                 NetworkError::ERROR_STATE_NONE, 1);
  histograms_.ExpectUniqueSample(kErrorShownSigninHistogram,
                                 NetworkError::ERROR_STATE_NONE, 1);
}

// Show 3 offline errors and 1 portal error. Make sure in time histograms logged
// portal error only.
TEST_F(ErrorScreensHistogramHelperTest, ShowScreenAndError) {
  enrollment_helper->OnScreenShow();
  signin_helper->OnScreenShow();
  enrollment_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  signin_helper->OnErrorShow(NetworkError::ERROR_STATE_PORTAL);
  enrollment_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  enrollment_helper->OnErrorHide();
  signin_helper->OnErrorHide();
  enrollment_helper->OnErrorShow(NetworkError::ERROR_STATE_OFFLINE);
  histograms_.ExpectUniqueSample(kErrorShownEnrollmentHistogram,
                                 NetworkError::ERROR_STATE_OFFLINE, 3);
  histograms_.ExpectUniqueSample(kErrorShownSigninHistogram,
                                 NetworkError::ERROR_STATE_PORTAL, 1);
  enrollment_helper->OnErrorShow(NetworkError::ERROR_STATE_PORTAL);
  histograms_.ExpectBucketCount(kErrorShownEnrollmentHistogram,
                                NetworkError::ERROR_STATE_PORTAL, 1);
  histograms_.ExpectTotalCount(kErrorTimeEnrollmentPortalHistogram, 0);
  enrollment_helper.reset();
  histograms_.ExpectTotalCount(kErrorTimeEnrollmentPortalHistogram, 1);
}

// Show error and hide it after 1 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowHideTime) {
  enrollment_helper->OnScreenShow();
  signin_helper->OnScreenShow();
  base::Time now = base::Time::Now();
  enrollment_helper->OnErrorShowTime(NetworkError::ERROR_STATE_PORTAL, now);
  now += base::Milliseconds(1000);
  enrollment_helper->OnErrorHideTime(now);
  enrollment_helper.reset();
  histograms_.ExpectUniqueSample(kErrorTimeEnrollmentPortalHistogram, 1000, 1);
}

// Show, hide, show, hide error with 1 sec interval. Make sure time logged in
// histogram is 2 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowHideShowHideTime) {
  enrollment_helper->OnScreenShow();
  signin_helper->OnScreenShow();
  base::Time now = base::Time::Now();
  enrollment_helper->OnErrorShowTime(NetworkError::ERROR_STATE_PROXY, now);
  now += base::Milliseconds(1000);
  enrollment_helper->OnErrorHideTime(now);
  now += base::Milliseconds(1000);
  enrollment_helper->OnErrorShowTime(NetworkError::ERROR_STATE_PORTAL, now);
  now += base::Milliseconds(1000);
  enrollment_helper->OnErrorHideTime(now);
  enrollment_helper.reset();
  histograms_.ExpectUniqueSample(kErrorTimeEnrollmentPortalHistogram, 2000, 1);
}

// Show, show, hide error with 1 sec interval. Make sure time logged in
// histogram is 2 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowShowHideTime) {
  user_creation_helper->OnScreenShow();
  signin_helper->OnScreenShow();
  base::Time now = base::Time::Now();
  user_creation_helper->OnErrorShowTime(NetworkError::ERROR_STATE_PORTAL, now);
  now += base::Milliseconds(1000);
  user_creation_helper->OnErrorShowTime(NetworkError::ERROR_STATE_PROXY, now);
  now += base::Milliseconds(1000);
  user_creation_helper->OnErrorHideTime(now);
  user_creation_helper.reset();
  histograms_.ExpectUniqueSample(kErrorTimeUserCreationProxyHistogram, 2000, 1);
}

}  // namespace ash
