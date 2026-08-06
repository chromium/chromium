// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/real_time_report_controller_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#endif
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"

namespace enterprise_reporting {

RealTimeReportControllerDelegate::RealTimeReportControllerDelegate(
    Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extension_request_observer_factory_ =
      std::make_unique<ExtensionRequestObserverFactory>(profile);
#endif
  LegacyTechServiceFactory::GetInstance()->SetReportTrigger(
      base::BindRepeating(&RealTimeReportControllerDelegate::TriggerLegacyTech,
                          weak_factory_.GetWeakPtr()));
}

RealTimeReportControllerDelegate::~RealTimeReportControllerDelegate() = default;

void RealTimeReportControllerDelegate::StartWatchingExtensionRequestIfNeeded() {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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
      &RealTimeReportControllerDelegate::TriggerExtensionRequest,
      weak_factory_.GetWeakPtr()));
#endif
}

void RealTimeReportControllerDelegate::StopWatchingExtensionRequest() {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extension_request_observer_factory_) {
    extension_request_observer_factory_->DisableReport();
  }
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
void RealTimeReportControllerDelegate::TriggerExtensionRequest(
    Profile* profile) {
  if (trigger_callback_) {
    trigger_callback_.Run(
        RealTimeReportType::kExtensionRequest,
        ExtensionRequestReportGenerator::ExtensionRequestData(profile));
  }
}
#endif

void RealTimeReportControllerDelegate::TriggerLegacyTech(
    LegacyTechReportGenerator::LegacyTechData data) {
  if (trigger_callback_) {
    trigger_callback_.Run(RealTimeReportType::kLegacyTech, std::move(data));
  }
}

}  // namespace enterprise_reporting
