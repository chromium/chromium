// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <utility>

#include "base/base64.h"
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
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
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
      bool need_encryption_key,
      std::unique_ptr<std::vector<EncryptedRecord>> records,
      policy::CloudPolicyClient* client,
      DmServerUploadService::CompletionCallback upload_complete_cb,
      DmServerUploadService::EncryptionKeyAttachedCallback
          encryption_key_attached_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

 private:
  ~ReportUploader() override;

  void OnStart() override;

  void StartUpload(bool need_encryption_key,
                   const EncryptedRecord& encrypted_record);
  void OnUploadComplete(base::Optional<base::Value> response);
  void HandleFailedUpload();
  void HandleSuccessfulUpload();

  // Populates upload request. Returns JSON request base::Value or nullopt,
  // if an error was detected.
  base::Optional<base::Value> PopulateRequest(
      bool need_encryption_key,
      const EncryptedRecord& encrypted_record);

  void Complete(DmServerUploadService::CompletionResponse result);

  const bool need_encryption_key_;
  std::unique_ptr<std::vector<EncryptedRecord>> records_;
  policy::CloudPolicyClient* client_;

  // Encryption key delivery callback.
  DmServerUploadService::EncryptionKeyAttachedCallback
      encryption_key_attached_cb_;

  // Last successful response to be processed.
  // Note: I could not find a way to pass it as a parameter,
  // so it is a class member variable. |last_response_| must be processed before
  // any attempt to retry calling the client, otherwise it will be overwritten.
  base::Value last_response_;

  // Set for the highest record being uploaded.
  base::Optional<SequencingInformation> highest_sequencing_information_;
};

RecordHandlerImpl::ReportUploader::ReportUploader(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    policy::CloudPolicyClient* client,
    DmServerUploadService::CompletionCallback client_cb,
    DmServerUploadService::EncryptionKeyAttachedCallback
        encryption_key_attached_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<DmServerUploadService::CompletionResponse>(
          std::move(client_cb),
          sequenced_task_runner),
      need_encryption_key_(need_encryption_key),
      records_(std::move(records)),
      client_(client),
      encryption_key_attached_cb_(encryption_key_attached_cb) {}

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

  StartUpload(need_encryption_key_, records_->back());
}

void RecordHandlerImpl::ReportUploader::StartUpload(
    bool need_encryption_key,
    const EncryptedRecord& encrypted_record) {
  auto response_cb =
      base::BindOnce(&RecordHandlerImpl::ReportUploader::OnUploadComplete,
                     base::Unretained(this));

  auto request_result =
      UploadEncryptedReportingRequestBuilder(need_encryption_key)
          .AddRecord(encrypted_record)
          .Build();
  if (!request_result.has_value()) {
    std::move(response_cb).Run(base::nullopt);
    return;
  }
  base::Value request = std::move(request_result.value());

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          [](policy::CloudPolicyClient* client, base::Value request,
             base::OnceCallback<void(base::Optional<base::Value>)>
                 response_cb) {
            client->UploadEncryptedReport(
                std::move(request),
                reporting::GetContext(ProfileManager::GetPrimaryUserProfile()),
                std::move(response_cb));
          },
          client_, std::move(request), std::move(response_cb)));
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
  //    "encryptionSettings": ... // EncryptionSettings proto
  //  }
  // TODO(b/169883262): Factor out the decoding into a separate class.

  const base::Value* last_succeed_uploaded_record =
      last_response_.FindDictKey("lastSucceedUploadedRecord");
  if (last_succeed_uploaded_record != nullptr) {
    // Note: Fields below are 'int', should be converted into 'uint64_t'.
    const std::string* sequencing_id_str =
        last_succeed_uploaded_record->FindStringKey("sequencingId");
    const std::string* generation_id_str =
        last_succeed_uploaded_record->FindStringKey("generationId");
    const auto priority = last_succeed_uploaded_record->FindIntKey("priority");
    uint64_t sequencing_id = 0;
    uint64_t generation_id = 0;
    if (sequencing_id_str &&
        base::StringToUint64(*sequencing_id_str, &sequencing_id) &&
        generation_id_str &&
        base::StringToUint64(*generation_id_str, &generation_id) &&
        priority.has_value() && Priority_IsValid(priority.value())) {
      SequencingInformation seq_info;
      seq_info.set_sequencing_id(sequencing_id);
      seq_info.set_generation_id(generation_id);
      seq_info.set_priority(Priority(priority.value()));
      highest_sequencing_information_ = std::move(seq_info);
    }
  }
  // TODO(b/169883262): Decode and handle failure information.

  // Handle the encryption settings.
  // Note: server can attach it to response regardless of whether
  // the response indicates success or failure, and whether the client
  // set attach_encryption_settings to true in request.
  const base::Value* signed_encryption_key_record =
      last_response_.FindDictKey("encryptionSettings");
  if (signed_encryption_key_record != nullptr) {
    const std::string* public_key_str =
        signed_encryption_key_record->FindStringKey("publicKey");
    const auto public_key_id_result =
        signed_encryption_key_record->FindIntKey("publicKeyId");
    // TODO(b/170054326): Make signature mandatory too.
    // const std::string* public_key_signature_str =
    //     signed_encryption_key_record->FindStringKey("publicKeySignature");
    std::string public_key;
    std::string public_key_signature;
    if (public_key_str != nullptr &&
        base::Base64Decode(*public_key_str, &public_key) &&
        // TODO(b/170054326): Make signature mandatory too.
        // public_key_signature_str != nullptr
        // base::Base64Decode(*public_key_signature_str,
        //                    &public_key_signature) &&
        public_key_id_result.has_value()) {
      SignedEncryptionInfo signed_encryption_key;
      signed_encryption_key.set_public_asymmetric_key(public_key);
      signed_encryption_key.set_public_key_id(public_key_id_result.value());
      signed_encryption_key.set_signature(public_key_signature);
      encryption_key_attached_cb_.Run(signed_encryption_key);
    }
  }

  // Pop the last record that was processed.
  records_->pop_back();

  if (records_->empty()) {
    Complete(highest_sequencing_information_.value());
    return;
  }

  // Upload the next record but do not request encryption key again.
  StartUpload(/*need_encryption_key=*/false, records_->back());
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
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    DmServerUploadService::CompletionCallback upload_complete_cb,
    DmServerUploadService::EncryptionKeyAttachedCallback
        encryption_key_attached_cb) {
  Start<RecordHandlerImpl::ReportUploader>(
      need_encryption_key, std::move(records), GetClient(),
      std::move(upload_complete_cb), encryption_key_attached_cb,
      sequenced_task_runner_);
}

}  // namespace reporting
