// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"
#endif
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"

class Profile;

namespace enterprise_reporting {

class RealTimeReportControllerDelegate
    : public RealTimeReportController::Delegate {
 public:
  explicit RealTimeReportControllerDelegate(Profile* profile = nullptr);
  RealTimeReportControllerDelegate(const RealTimeReportControllerDelegate&) =
      delete;
  RealTimeReportControllerDelegate& operator=(
      const RealTimeReportControllerDelegate&) = delete;
  ~RealTimeReportControllerDelegate() override;

  // RealTimeReportController::Delegate
  void StartWatchingExtensionRequestIfNeeded() override;
  void StopWatchingExtensionRequest() override;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  void TriggerExtensionRequest(Profile* profile);
#endif
  void TriggerLegacyTech(LegacyTechReportGenerator::LegacyTechData data);

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  std::unique_ptr<ExtensionRequestObserverFactory>
      extension_request_observer_factory_;
#endif

  base::WeakPtrFactory<RealTimeReportControllerDelegate> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_CONTROLLER_DELEGATE_H_
