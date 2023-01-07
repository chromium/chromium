// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/parental_control_metrics.h"

#include "base/check.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"

ParentalControlMetrics::ParentalControlMetrics(SupervisedUserService* service)
    : supervised_user_service_(service) {
  DCHECK(supervised_user_service_);
}

ParentalControlMetrics::~ParentalControlMetrics() = default;

void ParentalControlMetrics::OnNewDay() {
  // Ignores the first report during OOBE. Prefs related to web filter
  // policy may not have been successfully sync during OOBE process, which
  // introduces bias.
  if (first_report_on_current_device_) {
    first_report_on_current_device_ = false;
  } else {
    // Ignores reports when web filter prefs are reset to default value. It
    // might happen during sign out.
    supervised_user_service_->ReportNonDefaultWebFilterValue();
  }
}
