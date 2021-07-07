// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"

#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

namespace {

using enterprise_connectors::DeviceTrustSignalReporter;

// Wrap the callbacks to convert bool to Status.
reporting::ReportQueue::EnqueueCallback MakeEnqueueCallback(
    DeviceTrustSignalReporter::Callback sent_cb) {
  return base::BindOnce(
      [](decltype(sent_cb) cb, reporting::Status status) {
        DCHECK(status.ok()) << status;
        std::move(cb).Run(status.ok());
      },
      std::move(sent_cb));
}

reporting::ReportQueueConfiguration::PolicyCheckCallback
MakePolicyCheckCallback(base::RepeatingCallback<bool()> policy_check) {
  return base::BindRepeating(
      [=](decltype(policy_check) check) {
        using reporting::Status;
        constexpr auto err_code = reporting::error::PERMISSION_DENIED;
        return check.Run() ? Status::StatusOK()
                           : Status(err_code, "Disallowed per policy");
      },
      policy_check);
}

}  // namespace

namespace enterprise_connectors {

DeviceTrustSignalReporter::DeviceTrustSignalReporter()
    : create_queue_function_(
          base::BindOnce(&reporting::ReportQueueProvider::CreateQueue)) {}

DeviceTrustSignalReporter::~DeviceTrustSignalReporter() = default;

void DeviceTrustSignalReporter::Init(
    base::RepeatingCallback<bool()> policy_check,
    Callback done_cb) {
  switch (create_queue_status_) {
    case CreateQueueStatus::NOT_STARTED: {
      DCHECK(create_queue_function_);
      create_queue_status_ = CreateQueueStatus::IN_PROGRESS;
      break;  // Break out to go on to create the ReportQueue.
    }
    case CreateQueueStatus::IN_PROGRESS: {
      NOTREACHED();
      return;
    }
    case CreateQueueStatus::DONE: {
      // reporting::ReportQueueProvider::CreateQueue should not be retried if
      // previously failed; need browser restart or further investigation about
      // why creation of ReportQueue failed.
      std::move(done_cb).Run(report_queue_.get() != nullptr);
      return;
    }
  }
  // No default case so that compiler will complain if there's any new enums
  // values added.

  // Create ReportQueueConfiguration.
  policy::DMToken dm_token = GetDmToken();
  QueueConfigStatusOr config_result;
  if (dm_token.is_valid()) {
    config_result = CreateQueueConfiguration(dm_token.value(), policy_check);
  }
  // Bail out if ReportQueueConfiguration creation failed.
  if (!config_result.ok()) {
    LOG(ERROR) << "Failed to create reporting::ReportQueueConfiguration: "
               << config_result.status() << "; DM token: "
               << (dm_token.is_valid() ? dm_token.value() : "<invalid token>");
    create_queue_status_ = CreateQueueStatus::DONE;
    std::move(done_cb).Run(false);
    return;
  }

  // Wrap to convert reporting::ReportQueueProvider::CreateReportQueueResponse
  // to bool for done_cb.
  auto create_queue_cb =
      base::BindOnce(&DeviceTrustSignalReporter::OnCreateReportQueueResponse,
                     weak_factory_.GetWeakPtr(), std::move(done_cb));
  // Asynchronously create ReportQueue.
  PostCreateReportQueueTask(std::move(config_result.ValueOrDie()),
                            std::move(create_queue_cb));
}

void DeviceTrustSignalReporter::SendReport(base::Value value,
                                           Callback sent_cb) const {
  CHECK_EQ(create_queue_status_, CreateQueueStatus::DONE);
  DCHECK(report_queue_);
  report_queue_->Enqueue(std::move(value), reporting::Priority::FAST_BATCH,
                         MakeEnqueueCallback(std::move(sent_cb)));
}

void DeviceTrustSignalReporter::SendReport(const DeviceTrustReportEvent* report,
                                           Callback sent_cb) const {
  CHECK_EQ(create_queue_status_, CreateQueueStatus::DONE);
  DCHECK(report_queue_);
  report_queue_->Enqueue(report, reporting::Priority::FAST_BATCH,
                         MakeEnqueueCallback(std::move(sent_cb)));
}

void DeviceTrustSignalReporter::OnCreateReportQueueResponse(
    Callback create_queue_cb,
    reporting::ReportQueueProvider::CreateReportQueueResponse queue_result) {
  bool success = queue_result.ok();
  if (success) {
    report_queue_ = std::move(queue_result.ValueOrDie());
  } else {
    LOG(ERROR) << "Failed to create ReportQueue: " << queue_result.status();
  }
  // Set to DONE even upon failure to prevent repeated queue creation.
  create_queue_status_ = CreateQueueStatus::DONE;

  std::move(create_queue_cb).Run(success);
}

policy::DMToken DeviceTrustSignalReporter::GetDmToken() const {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  return policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
#else
  return policy::DMToken();
#endif
}

DeviceTrustSignalReporter::QueueConfigStatusOr
DeviceTrustSignalReporter::CreateQueueConfiguration(
    const std::string& dm_token,
    base::RepeatingCallback<bool()> policy_check) const {
  return reporting::ReportQueueConfiguration::Create(
      dm_token, reporting::Destination::DEVICE_TRUST_REPORTS,
      MakePolicyCheckCallback(policy_check));
}

void DeviceTrustSignalReporter::PostCreateReportQueueTask(
    std::unique_ptr<QueueConfig> config,
    CreateQueueCallback create_queue_cb) {
  DCHECK(config);
  DCHECK(config->CheckPolicy().ok()) << "This will prevent reporting!";
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(create_queue_function_),
                                std::move(config), std::move(create_queue_cb)));
}

void DeviceTrustSignalReporter::SetQueueCreationForTesting(QueueCreation f) {
  create_queue_function_ = std::move(f);
}

}  // namespace enterprise_connectors
