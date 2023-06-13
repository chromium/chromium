// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/real_time_report_controller_desktop.h"

#include <memory>

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"

namespace enterprise_reporting {

RealTimeReportControllerDesktop::RealTimeReportControllerDesktop(
    Profile* profile)
    : extension_request_observer_factory_(
          std::make_unique<ExtensionRequestObserverFactory>(profile)) {}

RealTimeReportControllerDesktop::~RealTimeReportControllerDesktop() = default;

void RealTimeReportControllerDesktop::StartWatchingExtensionRequestIfNeeded() {
  if (!extension_request_observer_factory_) {
    return;
  }

  // On CrOS, the function may be called twice during startup.
  if (extension_request_observer_factory_->IsReportEnabled()) {
    return;
  }

  // Unretained is safe here as the callback will always be called synchronously
  // while the owner will be deleted before the controller.
  extension_request_observer_factory_->EnableReport(base::BindRepeating(
      &RealTimeReportControllerDesktop::TriggerExtensionRequest,
      base::Unretained(this)));
}

void RealTimeReportControllerDesktop::StopWatchingExtensionRequest() {
  if (extension_request_observer_factory_) {
    extension_request_observer_factory_->DisableReport();
  }
}

void RealTimeReportControllerDesktop::TriggerExtensionRequest(
    Profile* profile) {
  if (!trigger_callback_.is_null()) {
    trigger_callback_.Run(
        RealTimeReportController::ReportTrigger::kExtensionRequest,
        ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  }
}

}  // namespace enterprise_reporting
