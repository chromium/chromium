
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_encrypted_reporter.h"

#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/app_install_events.pb.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace policy {

ArcAppInstallEventEncryptedReporter::ArcAppInstallEventEncryptedReporter(
    std::unique_ptr<reporting::ReportQueue, base::OnTaskRunnerDeleter>
        report_queue,
    Profile* profile)
    : report_queue_(std::move(report_queue)),
      logger_(std::make_unique<ArcAppInstallEventLogger>(this, profile)) {
  CHECK(report_queue_);
}

ArcAppInstallEventEncryptedReporter::~ArcAppInstallEventEncryptedReporter() {
  logger_.reset();
}

// ArcAppInstallEventLogger::Delegate:
void ArcAppInstallEventEncryptedReporter::Add(
    const std::set<std::string>& packages,
    const enterprise_management::AppInstallReportLogEvent& event) {
  // Only report pending installations, successful installations, or failures.
  switch (event.event_type()) {
    case enterprise_management::AppInstallReportLogEvent::SUCCESS:
    case enterprise_management::AppInstallReportLogEvent::INSTALLATION_STARTED:
    case enterprise_management::AppInstallReportLogEvent::INSTALLATION_FAILED:
      // Report each package + event combination.
      for (const auto& package : packages) {
        auto android_app_install_event =
            std::make_unique<reporting::AndroidAppInstallEvent>(
                CreateAndroidAppInstallEvent(package, event));
        report_queue_->Enqueue(
            std::move(android_app_install_event),
            reporting::Priority::BACKGROUND_BATCH,
            base::BindOnce([](reporting::Status status) {
              LOG(ERROR) << "Arc app install event enqueue status = " << status;
            }));
      }
      break;
    default:
      break;
  }
}

void ArcAppInstallEventEncryptedReporter::GetAndroidId(
    ArcAppInstallEventLogger::Delegate::AndroidIdCallback callback) const {
  arc::GetAndroidId(std::move(callback));
}

}  // namespace policy
