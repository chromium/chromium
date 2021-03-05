// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/error_screens_histogram_helper.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace chromeos {

namespace {

const char kOobeErrorScreensCounterPrefix[] = "OOBE.NetworkErrorShown.";
const char kOobeTimeSpentOnErrorScreensPrefix[] = "OOBE.ErrorScreensTime.";

const int kTimeMinInMS = 10;
const int kTimeMaxInMinutes = 3;
const int kTimeBucketCount = 50;

std::string ErrorToString(NetworkError::ErrorState error) {
  switch (error) {
    case NetworkError::ERROR_STATE_PORTAL:
      return ".Portal";
    case NetworkError::ERROR_STATE_OFFLINE:
      return ".Offline";
    case NetworkError::ERROR_STATE_PROXY:
      return ".Proxy";
    case NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT:
      return ".AuthExtTimeout";
    default:
      NOTREACHED() << "Invalid ErrorState " << error;
      return std::string();
  }
}

void StoreErrorScreenToHistogram(const std::string& screen_name,
                                 NetworkError::ErrorState error) {
  if (error <= NetworkError::ERROR_STATE_UNKNOWN ||
      error > NetworkError::ERROR_STATE_NONE)
    return;
  std::string histogram_name = kOobeErrorScreensCounterPrefix + screen_name;
  int boundary = NetworkError::ERROR_STATE_NONE + 1;
  // This comes from UMA_HISTOGRAM_ENUMERATION macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, boundary, boundary + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(error);
}

void StoreTimeOnErrorScreenToHistogram(const std::string& screen_name,
                                       NetworkError::ErrorState error,
                                       const base::TimeDelta& time_delta) {
  if (error <= NetworkError::ERROR_STATE_UNKNOWN ||
      error > NetworkError::ERROR_STATE_NONE)
    return;
  std::string histogram_name =
      kOobeTimeSpentOnErrorScreensPrefix + screen_name + ErrorToString(error);

  // This comes from UMA_HISTOGRAM_MEDIUM_TIMES macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name, base::TimeDelta::FromMilliseconds(kTimeMinInMS),
      base::TimeDelta::FromMinutes(kTimeMaxInMinutes), kTimeBucketCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->AddTime(time_delta);
}

}  // namespace

ErrorScreensHistogramHelper::ErrorScreensHistogramHelper(
    const std::string& screen_name)
    : screen_name_(screen_name),
      was_shown_(false),
      last_error_shown_(NetworkError::ERROR_STATE_NONE) {}

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
  StoreErrorScreenToHistogram(screen_name_, error);
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
      StoreErrorScreenToHistogram(screen_name_, NetworkError::ERROR_STATE_NONE);
    } else {
      if (!error_screen_start_time_.is_null()) {
        time_on_error_screens_ += base::Time::Now() - error_screen_start_time_;
        error_screen_start_time_ = base::Time();
      }
      StoreTimeOnErrorScreenToHistogram(screen_name_, last_error_shown_,
                                        time_on_error_screens_);
    }
  }
}

}  // namespace chromeos
