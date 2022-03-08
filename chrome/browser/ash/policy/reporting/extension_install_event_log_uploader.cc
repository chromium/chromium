// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/extension_install_event_log_uploader.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_factory.h"

namespace em = enterprise_management;

namespace policy {

ExtensionInstallEventLogUploader::Delegate::~Delegate() {}

// static
std::unique_ptr<ExtensionInstallEventLogUploader>
ExtensionInstallEventLogUploader::Create(Profile* profile) {
  return base::WrapUnique(new ExtensionInstallEventLogUploader(
      profile, ::reporting::ReportQueueFactory::CreateSpeculativeReportQueue(
                   ::reporting::EventType::kUser,
                   ::reporting::Destination::UPLOAD_EVENTS)));
}

// static
std::unique_ptr<ExtensionInstallEventLogUploader>
ExtensionInstallEventLogUploader::CreateForTest(
    Profile* profile,
    std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
        report_queue) {
  return base::WrapUnique(
      new ExtensionInstallEventLogUploader(profile, std::move(report_queue)));
}

ExtensionInstallEventLogUploader::~ExtensionInstallEventLogUploader() = default;

void ExtensionInstallEventLogUploader::SetDelegate(Delegate* delegate) {
  if (delegate_)
    CancelUpload();
  delegate_ = delegate;
}

ExtensionInstallEventLogUploader::ExtensionInstallEventLogUploader(
    Profile* profile,
    std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
        report_queue)
    : InstallEventLogUploaderBase(profile),
      report_queue_(std::move(report_queue)) {}

void ExtensionInstallEventLogUploader::CancelClientUpload() {
  weak_factory_.InvalidateWeakPtrs();
}

void ExtensionInstallEventLogUploader::StartSerialization() {
  delegate_->SerializeExtensionLogForUpload(
      base::BindOnce(&ExtensionInstallEventLogUploader::EnqueueReport,
                     weak_factory_.GetWeakPtr()));
}

void ExtensionInstallEventLogUploader::CheckDelegateSet() {
  CHECK(delegate_);
}

void ExtensionInstallEventLogUploader::OnUploadSuccess() {
  delegate_->OnExtensionLogUploadSuccess();
}

void ExtensionInstallEventLogUploader::PostTaskForStartSerialization() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExtensionInstallEventLogUploader::StartSerialization,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(retry_backoff_ms_));
}

void ExtensionInstallEventLogUploader::EnqueueReport(
    const em::ExtensionInstallReportRequest* report) {
  base::Value::Dict context = ::reporting::GetContext(profile_);
  base::Value::List event_list = ConvertExtensionProtoToValue(report, context);

  base::Value::Dict value_report =
      RealtimeReportingJobConfiguration::BuildReport(std::move(event_list),
                                                     std::move(context));

  // If --extension-install-event-chrome-log-for-tests is present, write event
  // logs to Chrome log. LOG(ERROR) ensures that logs are written.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kExtensionInstallEventChromeLogForTests)) {
    for (const em::ExtensionInstallReport& extension_install_report :
         report->extension_install_reports()) {
      for (const em::ExtensionInstallReportLogEvent&
               extension_install_report_log_event :
           extension_install_report.logs()) {
        if (extension_install_report_log_event.has_event_type()) {
          LOG(ERROR) << "Add extension install event: "
                     << extension_install_report.extension_id() << ", "
                     << extension_install_report_log_event.event_type();
        }
      }
    }
  }

  // Uploader must be called on the correct thread, in order to achieve that we
  // pass the appropriate task_runner along with the call.
  auto on_enqueue_done_cb = base::BindOnce(
      [](base::WeakPtr<ExtensionInstallEventLogUploader> uploader,
         scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         ::reporting::Status status) {
        auto call_uploader_with_status = base::BindOnce(
            [](base::WeakPtr<ExtensionInstallEventLogUploader> uploader,
               const ::reporting::Status& status) {
              uploader->OnUploadDone(status.ok());
            },
            uploader, status);

        task_runner->PostTask(FROM_HERE, std::move(call_uploader_with_status));
      },
      weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get());

  report_queue_->Enqueue(value_report, ::reporting::Priority::SLOW_BATCH,
                         std::move(on_enqueue_done_cb));
}

}  // namespace policy
