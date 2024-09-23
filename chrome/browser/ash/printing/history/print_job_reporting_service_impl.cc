// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/ash/printing/history/print_job_reporting_service.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/util/status.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace print = ::ash::printing::proto;
namespace em = ::enterprise_management;

namespace ash {

class PrintJobReportingServiceImpl : public PrintJobReportingService {
 public:
  explicit PrintJobReportingServiceImpl(
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue)
      : report_queue_(std::move(report_queue)),
        cros_settings_(CrosSettings::Get()) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    UpdateShouldReport();
    should_report_subscription_ = cros_settings_->AddSettingsObserver(
        kReportDevicePrintJobs,
        base::BindRepeating(&PrintJobReportingServiceImpl::UpdateShouldReport,
                            weak_factory_.GetWeakPtr()));
  }

  ~PrintJobReportingServiceImpl() override = default;

  void OnPrintJobFinished(const print::PrintJobInfo& print_job_info) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!should_report_) {
      VLOG(1) << "Reporting disabled for print job: " << print_job_info.id();
      return;
    }
    if (print_job_info.printer().source() !=
        print::Printer::PrinterSource::Printer_PrinterSource_POLICY) {
      VLOG(1) << "Not a managed printer for print job: " << print_job_info.id();
      return;
    }
    if (!report_queue_) {
      VLOG(1) << "No report queue set";
      return;
    }

    em::PrintJobEvent event = Convert(print_job_info);
    VLOG(1) << "Enqueuing event for print job: "
            << event.job_configuration().id();
    Enqueue(std::move(event));
  }

 private:
  void UpdateShouldReport() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (cros_settings_->PrepareTrustedValues(base::BindOnce(
            &PrintJobReportingServiceImpl::UpdateShouldReport,
            weak_factory_.GetWeakPtr())) != CrosSettingsProvider::TRUSTED) {
      return;
    }
    cros_settings_->GetBoolean(kReportDevicePrintJobs, &should_report_);
  }

  void Enqueue(em::PrintJobEvent event) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    report_queue_->Enqueue(
        std::make_unique<em::PrintJobEvent>(std::move(event)),
        ::reporting::Priority::SLOW_BATCH, base::DoNothing());
  }

  static em::PrintJobEvent Convert(const print::PrintJobInfo& print_job_info) {
    em::PrintJobEvent print_job_event;
    // Print job configuration
    auto* job_config = print_job_event.mutable_job_configuration();
    job_config->set_id(print_job_info.id());
    job_config->set_title(print_job_info.title());
    job_config->set_status(ConvertStatus(print_job_info.status()));
    job_config->set_creation_timestamp_ms(print_job_info.creation_time());
    job_config->set_completion_timestamp_ms(print_job_info.completion_time());
    job_config->set_number_of_pages(print_job_info.number_of_pages());
    // Print settings
    auto* settings = job_config->mutable_settings();
    settings->set_color(ConvertColor(print_job_info.settings().color()));
    settings->set_duplex(ConvertDuplex(print_job_info.settings().duplex()));
    *settings->mutable_media_size() =
        ConvertMediaSize(print_job_info.settings().media_size());
    settings->set_copies(print_job_info.settings().copies());
    // User
    auto* user_manager = user_manager::UserManager::Get();
    bool is_kiosk = user_manager->IsLoggedInAsAnyKioskApp();
    bool is_guest = user_manager->IsLoggedInAsGuest();
    print_job_event.set_user_type(is_kiosk   ? em::PrintJobEvent::KIOSK
                                  : is_guest ? em::PrintJobEvent::GUEST
                                             : em::PrintJobEvent::REGULAR);
    // Printer
    auto* printer = print_job_event.mutable_printer();
    printer->set_uri(print_job_info.printer().uri());
    printer->set_name(print_job_info.printer().name());
    printer->set_id(print_job_info.printer().id());
    return print_job_event;
  }

  static ::reporting::error::Code ConvertStatus(
      print::PrintJobInfo::PrintJobStatus status) {
    switch (status) {
      case print::PrintJobInfo::FAILED:
        return ::reporting::error::FAILED_PRECONDITION;
      case print::PrintJobInfo::CANCELED:
        return ::reporting::error::CANCELLED;
      case print::PrintJobInfo::PRINTED:
        return ::reporting::error::OK;
      default:
        return ::reporting::error::UNKNOWN;
    }
  }

  static em::PrintJobEvent::PrintSettings::ColorMode ConvertColor(
      print::PrintSettings::ColorMode color) {
    switch (color) {
      case print::PrintSettings::BLACK_AND_WHITE:
        return em::PrintJobEvent::PrintSettings::BLACK_AND_WHITE;
      case print::PrintSettings::COLOR:
        return em::PrintJobEvent::PrintSettings::COLOR;
      default:
        return em::PrintJobEvent::PrintSettings::UNKNOWN_COLOR_MODE;
    }
  }

  static em::PrintJobEvent::PrintSettings::DuplexMode ConvertDuplex(
      print::PrintSettings::DuplexMode color) {
    switch (color) {
      case print::PrintSettings::ONE_SIDED:
        return em::PrintJobEvent::PrintSettings::ONE_SIDED;
      case print::PrintSettings::TWO_SIDED_LONG_EDGE:
        return em::PrintJobEvent::PrintSettings::TWO_SIDED_LONG_EDGE;
      case print::PrintSettings::TWO_SIDED_SHORT_EDGE:
        return em::PrintJobEvent::PrintSettings::TWO_SIDED_SHORT_EDGE;
      default:
        return em::PrintJobEvent::PrintSettings::UNKNOWN_DUPLEX_MODE;
    }
  }

  static em::PrintJobEvent::PrintSettings::MediaSize ConvertMediaSize(
      const print::MediaSize& media_size) {
    em::PrintJobEvent::PrintSettings::MediaSize reporting_media_size;
    reporting_media_size.set_width(media_size.width());
    reporting_media_size.set_height(media_size.height());
    reporting_media_size.set_vendor_id(media_size.vendor_id());
    return reporting_media_size;
  }

  // Whether print jobs should be reported.
  bool should_report_ = false;

  // Subscription for the CrOS setting that determines whether print jobs should
  // be reported.
  base::CallbackListSubscription should_report_subscription_;

  // Speculative report queue for print jobs
  const std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
      report_queue_;

  const raw_ptr<CrosSettings> cros_settings_;

  base::WeakPtrFactory<PrintJobReportingServiceImpl> weak_factory_{this};
};

// static
std::unique_ptr<PrintJobReportingService> PrintJobReportingService::Create() {
  ::reporting::SourceInfo source_info;
  source_info.set_source(::reporting::SourceInfo::ASH);
  auto report_queue =
      ::reporting::ReportQueueFactory::CreateSpeculativeReportQueue(
          ::reporting::ReportQueueConfiguration::Create(
              {.event_type = ::reporting::EventType::kUser,
               .destination = ::reporting::Destination::PRINT_JOBS})
              .SetSourceInfo(std::move(source_info)));
  return std::make_unique<PrintJobReportingServiceImpl>(
      std::move(report_queue));
}

// static
std::unique_ptr<PrintJobReportingService>
PrintJobReportingService::CreateForTest(
    std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
        report_queue) {
  return std::make_unique<PrintJobReportingServiceImpl>(
      std::move(report_queue));
}

}  // namespace ash
