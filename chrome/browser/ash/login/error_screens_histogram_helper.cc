// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/error_screens_histogram_helper.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace ash {
namespace {

const char kOobeErrorScreensCounterPrefix[] = "OOBE.NetworkErrorShown.";
const char kOobeTimeSpentOnErrorScreensPrefix[] = "OOBE.ErrorScreensTime.";

const int kTimeMinInMS = 10;
const int kTimeMaxInMinutes = 3;
const int kTimeBucketCount = 50;

}  // namespace

std::string ErrorScreensHistogramHelper::GetParentScreenString() {
  switch (parent_screen_) {
    case ErrorParentScreen::kEnrollment:
      return "Enrollment";
    case ErrorParentScreen::kSignin:
      return "Signin";
    case ErrorParentScreen::kUpdate:
      return "Update";
    case ErrorParentScreen::kUpdateRequired:
      return "UpdateRequired";
    case ErrorParentScreen::kUserCreation:
      return "UserCreation";
    default:
      NOTREACHED() << "Invalid ErrorParentScreen "
                   << static_cast<int>(parent_screen_);
      return std::string();
  }
}

std::string ErrorScreensHistogramHelper::GetLastErrorShownString() {
  switch (last_error_shown_) {
    case NetworkError::ERROR_STATE_PORTAL:
      return ".Portal";
    case NetworkError::ERROR_STATE_OFFLINE:
      return ".Offline";
    case NetworkError::ERROR_STATE_PROXY:
      return ".Proxy";
    case NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT:
      return ".AuthExtTimeout";
    default:
      NOTREACHED() << "Invalid ErrorState " << last_error_shown_;
      return std::string();
  }
}

void ErrorScreensHistogramHelper::StoreErrorScreenToHistogram() {
  if (last_error_shown_ <= NetworkError::ERROR_STATE_UNKNOWN ||
      last_error_shown_ > NetworkError::ERROR_STATE_NONE)
    return;
  std::string histogram_name =
      kOobeErrorScreensCounterPrefix + GetParentScreenString();
  int boundary = NetworkError::ERROR_STATE_NONE + 1;
  // This comes from UMA_HISTOGRAM_ENUMERATION macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, boundary, boundary + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(last_error_shown_);
}

void ErrorScreensHistogramHelper::StoreTimeOnErrorScreenToHistogram(
    const base::TimeDelta& time_delta) {
  if (last_error_shown_ <= NetworkError::ERROR_STATE_UNKNOWN ||
      last_error_shown_ > NetworkError::ERROR_STATE_NONE)
    return;
  std::string histogram_name = kOobeTimeSpentOnErrorScreensPrefix +
                               GetParentScreenString() +
                               GetLastErrorShownString();

  // This comes from UMA_HISTOGRAM_MEDIUM_TIMES macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name, base::Milliseconds(kTimeMinInMS),
      base::Minutes(kTimeMaxInMinutes), kTimeBucketCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->AddTime(time_delta);
}

ErrorScreensHistogramHelper::ErrorScreensHistogramHelper(
    ErrorParentScreen parent_screen)
    : parent_screen_(parent_screen) {}

void ErrorScreensHistogramHelper::OnScreenShow() {
  was_shown_ = true;
}

void ErrorScreensHistogramHelper::OnErrorShow(NetworkError::ErrorState error) {
  OnErrorShowTime(error, base::Time::Now());
}

void ErrorScreensHistogramHelper::OnErrorShowTime(
    NetworkError::ErrorState error,
    base::Time now) {
  last_error_shown_ = error;
  if (error_screen_start_time_.is_null())
    error_screen_start_time_ = now;
  StoreErrorScreenToHistogram();
}

void ErrorScreensHistogramHelper::OnErrorHide() {
  OnErrorHideTime(base::Time::Now());
}

void ErrorScreensHistogramHelper::OnErrorHideTime(base::Time now) {
  if (error_screen_start_time_.is_null())
    return;
  time_on_error_screens_ += now - error_screen_start_time_;
  error_screen_start_time_ = base::Time();
}

ErrorScreensHistogramHelper::~ErrorScreensHistogramHelper() {
  if (was_shown_) {
    if (last_error_shown_ == NetworkError::ERROR_STATE_NONE) {
      StoreErrorScreenToHistogram();
    } else {
      if (!error_screen_start_time_.is_null()) {
        time_on_error_screens_ += base::Time::Now() - error_screen_start_time_;
        error_screen_start_time_ = base::Time();
      }
      StoreTimeOnErrorScreenToHistogram(time_on_error_screens_);
    }
  }
}

}  // namespace ash
