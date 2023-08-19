// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/network_error.h"

namespace ash {

// This class helps to track time user has spent on the ErrorScreen while
// going through `parent_screen`.
//
// This class records two histograms:
//   1. OOBE.NetworkErrorShown.{parent_screen}
//   2. OOBE.ErrorScreensTime.{parent_screen}.{error_state}
//
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
    kAddChild,
    kConsumerUpdate,
  };

  explicit ErrorScreensHistogramHelper(ErrorParentScreen parent_screen);
  ErrorScreensHistogramHelper(const ErrorScreensHistogramHelper&) = delete;
  ErrorScreensHistogramHelper& operator=(const ErrorScreensHistogramHelper&) =
      delete;
  ~ErrorScreensHistogramHelper();

  void OnScreenShow();
  void OnErrorShow(NetworkError::ErrorState error);
  void OnErrorHide();

 private:
  bool was_shown_ = false;
  ErrorParentScreen parent_screen_;
  NetworkError::ErrorState last_error_shown_ = NetworkError::ERROR_STATE_NONE;
  base::Time error_screen_start_time_;
  base::TimeDelta time_on_error_screens_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_
