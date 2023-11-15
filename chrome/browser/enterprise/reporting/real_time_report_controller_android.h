// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"

namespace enterprise_reporting {

class RealTimeReportControllerAndroid
    : public RealTimeReportController::Delegate {
 public:
  RealTimeReportControllerAndroid();
  RealTimeReportControllerAndroid(const RealTimeReportControllerAndroid&) =
      delete;
  RealTimeReportControllerAndroid& operator=(
      const RealTimeReportControllerAndroid&) = delete;
  ~RealTimeReportControllerAndroid() override;

  void TriggerLegacyTech(LegacyTechReportGenerator::LegacyTechData data);

 private:
  // RealTimeReportController::Delegate
  void StartWatchingExtensionRequestIfNeeded() override;
  void StopWatchingExtensionRequest() override;

  base::WeakPtrFactory<RealTimeReportControllerAndroid> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_ANDROID_H_
