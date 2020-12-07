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

// ReportUploader handles enqueuing events on the |report_queue_|,
// and uploading those events with the |client_|.
class RecordHandlerImpl::ReportUploader
    : public TaskRunnerContext<DmServerUploadService::CompletionResponse> {
 public:
  ReportUploader(
      std::unique_ptr<std::vector<EncryptedRecord>> records,
      policy::CloudPolicyClient* client,
      DmServerUploadService::CompletionCallback upload_complete_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

 private:
  ~ReportUploader() override;

  void OnStart() override;

  void StartUpload(const EncryptedRecord& encrypted_record);
  void OnUploadComplete(base::Optional<base::Value> response);
  void HandleFailedUpload();
  void HandleSuccessfulUpload();

  void Complete(DmServerUploadService::CompletionResponse result);

  std::unique_ptr<std::vector<EncryptedRecord>> records_;
  policy::CloudPolicyClient* client_;

  // Last successful response to be processed.
  // Note: I could not find a way to pass it as a parameter,
  // so it is a class member variable. |last_response_| must be processed before
  // any attempt to retry calling the client, otherwise it will be overwritten.
  base::Value last_response_;

  // Set for the highest record being uploaded.
  base::Optional<SequencingInformation> highest_sequencing_information_;
};

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
             base::OnceCallback<void(base::Optional<base::Value>)> cb) {
            client->UploadEncryptedReport(
                record,
                reporting::GetContext(ProfileManager::GetPrimaryUserProfile()),
                std::move(cb));
          },
          client_, encrypted_record, std::move(cb)));
}

void RecordHandlerImpl::ReportUploader::OnUploadComplete(
    base::Optional<base::Value> response) {
  if (!response.has_value()) {
    Schedule(&RecordHandlerImpl::ReportUploader::HandleFailedUpload,
             base::Unretained(this));
    return;
  }
  last_response_ = std::move(response.value());
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
  // Decypher 'response' containing a base::Value dictionary that looks like:
  //  {
  //    "lastSucceedUploadedRecord": ... // SequencingInformation proto
  //    "firstFailedUploadedRecord": {
  //      "failedUploadedRecord": ... // SequencingInformation proto
  //      "failureStatus": ... // Status proto
  //    }
  //  }
  // TODO(b/169883262): Factor out the decoding into a separate class.

  const base::Value* last_succeed_uploaded_record =
      last_response_.FindDictKey("lastSucceedUploadedRecord");
  if (last_succeed_uploaded_record != nullptr) {
    SequencingInformation seq_info;
    // Note: Fields below are 'int', should be converted into 'uint64_t'.
    const auto sequencing_id =
        last_succeed_uploaded_record->FindIntKey("sequencingId");
    const auto generation_id =
        last_succeed_uploaded_record->FindIntKey("generationId");
    const auto priority = last_succeed_uploaded_record->FindIntKey("priority");
    if (sequencing_id.has_value() && generation_id.has_value() &&
        priority.has_value() && Priority_IsValid(priority.value())) {
      seq_info.set_sequencing_id(sequencing_id.value());
      seq_info.set_generation_id(generation_id.value());
      seq_info.set_priority(Priority(priority.value()));
      highest_sequencing_information_ = std::move(seq_info);
    }
  }
  // TODO(b/169883262): Decode and handle failure information.
  // TODO(b/170054326): Handle the encryption settings.

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
