// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_PARENTAL_CONTROL_METRICS_H_
#define CHROME_BROWSER_SUPERVISED_USER_PARENTAL_CONTROL_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_metrics_service.h"

class SupervisedUserService;

// A class for recording time limit metrics and web filter metrics for Family
// Link users on Chrome browser at the beginning of the first active session
// daily.
class ParentalControlMetrics : public SupervisedUserMetricsService::Observer {
 public:
  explicit ParentalControlMetrics(SupervisedUserService* service);
  ParentalControlMetrics(const ParentalControlMetrics&) = delete;
  ParentalControlMetrics& operator=(const ParentalControlMetrics&) = delete;
  ~ParentalControlMetrics() override;

  // SupervisedUserMetricsService::Observer:
  void OnNewDay() override;

 private:
  const raw_ptr<SupervisedUserService> supervised_user_service_;
  bool first_report_on_current_device_ = false;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_PARENTAL_CONTROL_METRICS_H_
