// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/network_error.h"

namespace ash {

FORWARD_DECLARE_TEST(ErrorScreensHistogramHelperTest, TestShowHideTime);
FORWARD_DECLARE_TEST(ErrorScreensHistogramHelperTest, TestShowHideShowHideTime);
FORWARD_DECLARE_TEST(ErrorScreensHistogramHelperTest, TestShowShowHideTime);

class ErrorScreensHistogramHelper {
 public:
  // The screens that were shown when the error occurred.
  // This enum is tied to the `OOBEScreenShownBeforeNetworkError` variants in
  // //tools/metrics/histograms/metadata/oobe/histograms.xml. Do not change one
  // without changing the other.
  enum class ErrorParentScreen {
    kEnrollment,
    kSignin,
    kUpdate,
    kUpdateRequired,
    kUserCreation,
  };

  explicit ErrorScreensHistogramHelper(ErrorParentScreen parent_screen);
  void OnScreenShow();
  void OnErrorShow(NetworkError::ErrorState error);
  void OnErrorHide();
  ~ErrorScreensHistogramHelper();

 private:
  FRIEND_TEST_ALL_PREFIXES(ErrorScreensHistogramHelperTest, TestShowHideTime);
  FRIEND_TEST_ALL_PREFIXES(ErrorScreensHistogramHelperTest,
                           TestShowHideShowHideTime);
  FRIEND_TEST_ALL_PREFIXES(ErrorScreensHistogramHelperTest,
                           TestShowShowHideTime);
  // functions for testing.
  void OnErrorShowTime(NetworkError::ErrorState error, base::Time now);
  void OnErrorHideTime(base::Time now);

  std::string GetParentScreenString();
  std::string GetLastErrorShownString();
  void StoreErrorScreenToHistogram();
  void StoreTimeOnErrorScreenToHistogram(const base::TimeDelta& time_delta);

  bool was_shown_ = false;
  ErrorParentScreen parent_screen_;
  NetworkError::ErrorState last_error_shown_ = NetworkError::ERROR_STATE_NONE;
  base::Time error_screen_start_time_;
  base::TimeDelta time_on_error_screens_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_
