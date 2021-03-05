// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/network_error.h"

namespace chromeos {
FORWARD_DECLARE_TEST(ErrorScreensHistogramHelperTest, TestShowHideTime);
FORWARD_DECLARE_TEST(ErrorScreensHistogramHelperTest, TestShowHideShowHideTime);
FORWARD_DECLARE_TEST(ErrorScreensHistogramHelperTest, TestShowShowHideTime);

class ErrorScreensHistogramHelper {
 public:
  explicit ErrorScreensHistogramHelper(const std::string& screen_name);
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

  std::string screen_name_;
  bool was_shown_;
  NetworkError::ErrorState last_error_shown_;
  base::Time error_screen_start_time_;
  base::TimeDelta time_on_error_screens_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_ERROR_SCREENS_HISTOGRAM_HELPER_H_
