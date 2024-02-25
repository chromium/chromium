// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/real_time_report_controller_android.h"

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"

namespace enterprise_reporting {

RealTimeReportControllerAndroid::RealTimeReportControllerAndroid() {
  LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
      base::BindRepeating(&RealTimeReportControllerAndroid::TriggerLegacyTech,
                          weak_factory_.GetWeakPtr()));
}
RealTimeReportControllerAndroid::~RealTimeReportControllerAndroid() = default;

void RealTimeReportControllerAndroid::StartWatchingExtensionRequestIfNeeded() {
  // No-op because extensions are not supported on Android.
}

void RealTimeReportControllerAndroid::StopWatchingExtensionRequest() {
  // No-op because extensions are not supported on Android.
}

void RealTimeReportControllerAndroid::TriggerLegacyTech(
    LegacyTechReportGenerator::LegacyTechData data) {
  if (trigger_callback_) {
    trigger_callback_.Run(RealTimeReportType::kLegacyTech, std::move(data));
  }
}

}  // namespace enterprise_reporting
