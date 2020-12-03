// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

RecordHandlerImpl::ReportUploader::ReportUploader(
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    policy::CloudPolicyClient* client,
    DmServerUploadService::CompletionCallback client_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<DmServerUploadService::CompletionResponse>(
          std::move(client_cb),
          sequenced_task_runner),
      records_(std::move(records)),
      client_(client) {}

RecordHandlerImpl::ReportUploader::~ReportUploader() = default;

void RecordHandlerImpl::ReportUploader::OnStart() {
  if (client_ == nullptr) {
    Status null_client = Status(error::INVALID_ARGUMENT, "Client was null");
    LOG(ERROR) << null_client;
    Complete(null_client);
    return;
  }

  if (records_ == nullptr) {
    Status null_records = Status(error::INVALID_ARGUMENT, "records_ was null");
    LOG(ERROR) << null_records;
    Complete(null_records);
    return;
  }

  if (records_->empty()) {
    Status empty_records =
        Status(error::INVALID_ARGUMENT, "records_ was empty");
    LOG(ERROR) << empty_records;
    Complete(empty_records);
    return;
  }

  // We'll be popping records off the back.
  std::reverse(records_->begin(), records_->end());

  StartUpload(records_->back());
}

void RecordHandlerImpl::ReportUploader::StartUpload(
    const EncryptedRecord& encrypted_record) {
  auto cb = base::BindOnce(&RecordHandlerImpl::ReportUploader::OnUploadComplete,
                           base::Unretained(this));
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          [](policy::CloudPolicyClient* client, const EncryptedRecord& record,
             base::OnceCallback<void(bool)> cb) {
            client->UploadEncryptedReport(
                record,
                reporting::GetContext(ProfileManager::GetPrimaryUserProfile()),
                std::move(cb));
          },
          client_, encrypted_record, std::move(cb)));
}

void RecordHandlerImpl::ReportUploader::OnUploadComplete(bool success) {
  if (!success) {
    Schedule(&RecordHandlerImpl::ReportUploader::HandleFailedUpload,
             base::Unretained(this));
    return;
  }
  Schedule(&RecordHandlerImpl::ReportUploader::HandleSuccessfulUpload,
           base::Unretained(this));
}

void RecordHandlerImpl::ReportUploader::HandleFailedUpload() {
  Status data_loss = Status(
      error::DATA_LOSS,
      base::StrCat({"Record failed uploaded: ",
                    records_->back().sequencing_information().DebugString()}));
  LOG(ERROR) << data_loss;

  if (highest_sequencing_information_.has_value()) {
    Complete(std::move(highest_sequencing_information_.value()));
    return;
  }

  Complete(data_loss);
}

void RecordHandlerImpl::ReportUploader::HandleSuccessfulUpload() {
  highest_sequencing_information_ = records_->back().sequencing_information();

  // Pop the last record that was processed.
  records_->pop_back();

  if (records_->empty()) {
    Complete(highest_sequencing_information_.value());
    return;
  }

  StartUpload(records_->back());
}

void RecordHandlerImpl::ReportUploader::Complete(
    DmServerUploadService::CompletionResponse completion_result) {
  Schedule(&RecordHandlerImpl::ReportUploader::Response, base::Unretained(this),
           completion_result);
}

RecordHandlerImpl::RecordHandlerImpl(policy::CloudPolicyClient* client)
    : RecordHandler(client),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

RecordHandlerImpl::~RecordHandlerImpl() = default;

void RecordHandlerImpl::HandleRecords(
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    DmServerUploadService::CompletionCallback upload_complete_cb) {
  Start<RecordHandlerImpl::ReportUploader>(std::move(records), GetClient(),
                                           std::move(upload_complete_cb),
                                           sequenced_task_runner_);
}

}  // namespace reporting
