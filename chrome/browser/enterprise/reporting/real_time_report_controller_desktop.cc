// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/real_time_report_controller_desktop.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"

namespace enterprise_reporting {

RealTimeReportControllerDesktop::RealTimeReportControllerDesktop(
    Profile* profile)
    : extension_request_observer_factory_(
          std::make_unique<ExtensionRequestObserverFactory>(profile)) {
  LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
      base::BindRepeating(&RealTimeReportControllerDesktop::TriggerLegacyTech,
                          weak_factory_.GetWeakPtr()));
}

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
      weak_factory_.GetWeakPtr()));
}

void RealTimeReportControllerDesktop::StopWatchingExtensionRequest() {
  if (extension_request_observer_factory_) {
    extension_request_observer_factory_->DisableReport();
  }
}

void RealTimeReportControllerDesktop::TriggerExtensionRequest(
    Profile* profile) {
  if (trigger_callback_) {
    trigger_callback_.Run(
        RealTimeReportType::kExtensionRequest,
        ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  }
}

void RealTimeReportControllerDesktop::TriggerLegacyTech(
    LegacyTechReportGenerator::LegacyTechData data) {
  if (trigger_callback_) {
    trigger_callback_.Run(RealTimeReportType::kLegacyTech, std::move(data));
  }
}

}  // namespace enterprise_reporting
