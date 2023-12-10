// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"

namespace enterprise_reporting {

class RealTimeReportControllerDesktop
    : public RealTimeReportController::Delegate {
 public:
  explicit RealTimeReportControllerDesktop(Profile* profile = nullptr);
  RealTimeReportControllerDesktop(const RealTimeReportControllerDesktop&) =
      delete;
  RealTimeReportControllerDesktop& operator=(
      const RealTimeReportControllerDesktop&) = delete;
  ~RealTimeReportControllerDesktop() override;

  // RealTimeReportController::Delegate
  void StartWatchingExtensionRequestIfNeeded() override;
  void StopWatchingExtensionRequest() override;

  void TriggerExtensionRequest(Profile* profile);
  void TriggerLegacyTech(LegacyTechReportGenerator::LegacyTechData data);

 private:
  std::unique_ptr<ExtensionRequestObserverFactory>
      extension_request_observer_factory_;

  base::WeakPtrFactory<RealTimeReportControllerDesktop> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_DESKTOP_H_
