// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

namespace enterprise_connectors {

DeviceTrustSignalReporter::DeviceTrustSignalReporter() = default;
DeviceTrustSignalReporter::~DeviceTrustSignalReporter() = default;

void DeviceTrustSignalReporter::Init(
    base::RepeatingCallback<bool(void)> policy_check,
    base::OnceCallback<void(bool)> done_cb) {
  switch (create_queue_status_) {
    case CreateQueueStatus::NOT_STARTED: {
      create_queue_status_ = CreateQueueStatus::IN_PROGRESS;
      break;  // Break out to continue to create the ReportQueue.
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

  // Wrap to convert bool to Status.
  reporting::ReportQueueConfiguration::PolicyCheckCallback policy_cb =
      base::BindRepeating(
          [](decltype(policy_check) check) {
            return check.Run()
                       ? reporting::Status::StatusOK()
                       : reporting::Status(reporting::error::PERMISSION_DENIED,
                                           "Disallowed per policy");
          },
          policy_check);
  // Create ReportQueueConfiguration.
  policy::DMToken dm_token = GetDmToken();
  if (!dm_token.is_valid()) {
    LOG(ERROR) << "Failed to retrieve valid DM token";
    create_queue_status_ = CreateQueueStatus::DONE;
    std::move(done_cb).Run(false);
    return;
  }
  auto config_result = reporting::ReportQueueConfiguration::Create(
      dm_token.value(), reporting::Destination::DEVICE_TRUST_REPORTS,
      std::move(policy_cb));

  // Bail out if ReportQueueConfiguration creation failed.
  if (!config_result.ok()) {
    LOG(ERROR) << "Failed to create reporting::ReportQueueConfiguration: "
               << config_result.status() << "; DM token: " << dm_token.value();
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
  PostCreateReportQueueTask(std::move(create_queue_cb),
                            std::move(config_result.ValueOrDie()));
}

void DeviceTrustSignalReporter::SendReport(
    base::Value value,
    base::OnceCallback<void(bool)> sent_cb) {
  CHECK_EQ(create_queue_status_, CreateQueueStatus::DONE);
  DCHECK(report_queue_);

  reporting::ReportQueue::EnqueueCallback cb = base::BindOnce(
      [](decltype(sent_cb) cb, reporting::Status status) {
        std::move(cb).Run(status.ok());
      },
      std::move(sent_cb));
  report_queue_->Enqueue(std::move(value), reporting::Priority::FAST_BATCH,
                         std::move(cb));
}

void DeviceTrustSignalReporter::OnCreateReportQueueResponse(
    base::OnceCallback<void(bool)> create_queue_cb,
    reporting::ReportQueueProvider::CreateReportQueueResponse
        report_queue_result) {
  bool success = report_queue_result.ok();
  if (success) {
    report_queue_ = std::move(report_queue_result.ValueOrDie());
  } else {
    LOG(ERROR) << "Failed to create ReportQueue: "
               << report_queue_result.status();
  }
  create_queue_status_ = CreateQueueStatus::DONE;
  // Set to DONE even upon failure to prevent repeated queue creation.

  std::move(create_queue_cb).Run(success);
}

policy::DMToken DeviceTrustSignalReporter::GetDmToken() const {
  return policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
}

void DeviceTrustSignalReporter::PostCreateReportQueueTask(
    reporting::ReportQueueProvider::CreateReportQueueCallback create_queue_cb,
    std::unique_ptr<reporting::ReportQueueConfiguration> config) {
  auto create_queue_task =
      base::BindOnce(&reporting::ReportQueueProvider::CreateQueue,
                     std::move(config), std::move(create_queue_cb));
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, std::move(create_queue_task));
}

}  // namespace enterprise_connectors
