
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/policy/reporting/arc_app_install_encrypted_event_reporter.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/app_install_events.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace em = enterprise_management;

namespace policy {

ArcAppInstallEncryptedEventReporter::ArcAppInstallEncryptedEventReporter(
    std::unique_ptr<reporting::ReportQueue, base::OnTaskRunnerDeleter>
        report_queue,
    Profile* profile)
    : report_queue_(std::move(report_queue)),
      logger_(std::make_unique<ArcAppInstallEventLogger>(this, profile)) {
  CHECK(report_queue_);
}

ArcAppInstallEncryptedEventReporter::~ArcAppInstallEncryptedEventReporter() {
  logger_.reset();
}

// ArcAppInstallEventLogger::Delegate:
void ArcAppInstallEncryptedEventReporter::Add(
    const std::set<std::string>& packages,
    const em::AppInstallReportLogEvent& event) {
  // Only report pending installations, successful installations, or failures.
  switch (event.event_type()) {
    case em::AppInstallReportLogEvent::SUCCESS:
    case em::AppInstallReportLogEvent::INSTALLATION_STARTED:
    case em::AppInstallReportLogEvent::INSTALLATION_FAILED:
    case em::AppInstallReportLogEvent::SERVER_REQUEST:
      // Report each package + event combination.
      for (const auto& package : packages) {
        auto android_app_install_event =
            std::make_unique<reporting::AndroidAppInstallEvent>(
                CreateAndroidAppInstallEvent(package, event));
        report_queue_->Enqueue(
            std::move(android_app_install_event),
            reporting::Priority::BACKGROUND_BATCH,
            base::BindOnce(
                [](std::string package,
                   em::AppInstallReportLogEvent::EventType event_type,
                   reporting::Status status) {
                  // Do not remove this log. It's used in the
                  // `managed_app_install_logging.go` TAST test to verify events
                  // are reported. LOG(ERROR) ensures that logs are written
                  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                          ash::switches::kArcInstallEventChromeLogForTests)) {
                    LOG(ERROR)
                        << "Enqueued ARC install event: package = " << package
                        << " event type = " << event_type
                        << " enqueue status = " << status;
                  }
                },
                package, event.event_type()));
      }
      break;
    default:
      break;
  }
}

void ArcAppInstallEncryptedEventReporter::GetAndroidId(
    ArcAppInstallEventLogger::Delegate::AndroidIdCallback callback) const {
  arc::GetAndroidId(std::move(callback));
}

}  // namespace policy
