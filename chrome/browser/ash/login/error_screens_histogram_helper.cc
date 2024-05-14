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

std::string GetParentScreenString(
    ErrorScreensHistogramHelper::ErrorParentScreen screen) {
  switch (screen) {
    case ErrorScreensHistogramHelper::ErrorParentScreen::kEnrollment:
      return "Enrollment";
    case ErrorScreensHistogramHelper::ErrorParentScreen::kSignin:
      return "Signin";
    case ErrorScreensHistogramHelper::ErrorParentScreen::kUpdate:
      return "Update";
    case ErrorScreensHistogramHelper::ErrorParentScreen::kUpdateRequired:
      return "UpdateRequired";
    case ErrorScreensHistogramHelper::ErrorParentScreen::kUserCreation:
      return "UserCreation";
    case ErrorScreensHistogramHelper::ErrorParentScreen::kAddChild:
      return "AddChild";
    case ErrorScreensHistogramHelper::ErrorParentScreen::kConsumerUpdate:
      return "ConsumerUpdate";
  }
}

std::string GetErrorStateString(NetworkError::ErrorState state) {
  switch (state) {
    case NetworkError::ERROR_STATE_PORTAL:
      return ".Portal";
    case NetworkError::ERROR_STATE_OFFLINE:
      return ".Offline";
    case NetworkError::ERROR_STATE_PROXY:
      return ".Proxy";
    case NetworkError::ERROR_STATE_LOADING_TIMEOUT:
      return ".LoadingTimeout";
    case NetworkError::ERROR_STATE_NONE:
      return ".None";
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid ErrorState " << state;
      return std::string();
  }
}

void RecordErrorStateCountOnParentScreen(
    NetworkError::ErrorState state,
    ErrorScreensHistogramHelper::ErrorParentScreen screen) {
  if (state <= NetworkError::ERROR_STATE_UNKNOWN ||
      state > NetworkError::ERROR_STATE_NONE) {
    return;
  }

  std::string histogram_name =
      kOobeErrorScreensCounterPrefix + GetParentScreenString(screen);
  int boundary = NetworkError::ERROR_STATE_NONE + 1;

  // This comes from UMA_HISTOGRAM_ENUMERATION macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, boundary, boundary + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(state);
}

void RecordErrorScreenTimeOnParentScreenWithLastError(
    const base::TimeDelta& time_delta,
    ErrorScreensHistogramHelper::ErrorParentScreen screen,
    NetworkError::ErrorState state) {
  if (state <= NetworkError::ERROR_STATE_UNKNOWN ||
      state > NetworkError::ERROR_STATE_NONE) {
    return;
  }

  std::string histogram_name = kOobeTimeSpentOnErrorScreensPrefix +
                               GetParentScreenString(screen) +
                               GetErrorStateString(state);

  // This comes from UMA_HISTOGRAM_MEDIUM_TIMES macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name, base::Milliseconds(kTimeMinInMS),
      base::Minutes(kTimeMaxInMinutes), kTimeBucketCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->AddTime(time_delta);
}

}  // namespace

ErrorScreensHistogramHelper::ErrorScreensHistogramHelper(
    ErrorParentScreen parent_screen)
    : parent_screen_(parent_screen) {}

void ErrorScreensHistogramHelper::OnScreenShow() {
  was_shown_ = true;
}

void ErrorScreensHistogramHelper::OnErrorShow(NetworkError::ErrorState error) {
  // Don't record anything if the same error is shown.
  if (last_error_shown_ == error) {
    return;
  }
  // ERROR_STATE_NONE represents that the ErrorScreen was hidden or not shown.
  // Start recording time at this point.
  if (last_error_shown_ == NetworkError::ErrorState::ERROR_STATE_NONE) {
    error_screen_start_time_ = base::Time::Now();
  }
  // New error state is shown so we want to record it.
  last_error_shown_ = error;
  RecordErrorStateCountOnParentScreen(last_error_shown_, parent_screen_);
}

void ErrorScreensHistogramHelper::OnErrorHide() {
  if (error_screen_start_time_.is_null()) {
    return;
  }
  // Reset last error to represent that the ErrorScreen is hidden.
  last_error_shown_ = NetworkError::ErrorState::ERROR_STATE_NONE;

  // Increase the time spent on the ErrorScreen.
  time_on_error_screens_ += base::Time::Now() - error_screen_start_time_;
  error_screen_start_time_ = base::Time();
}

ErrorScreensHistogramHelper::~ErrorScreensHistogramHelper() {
  if (!was_shown_) {
    return;
  }

  if (!error_screen_start_time_.is_null()) {
    time_on_error_screens_ += base::Time::Now() - error_screen_start_time_;
    error_screen_start_time_ = base::Time();
  }
  RecordErrorScreenTimeOnParentScreenWithLastError(
      time_on_error_screens_, parent_screen_, last_error_shown_);
}

}  // namespace ash
