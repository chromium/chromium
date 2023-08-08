// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/token.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/management_utils.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_uploader.h"
#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace {

// Priority could come back as an int or as a std::string, this function handles
// both situations.
static absl::optional<Priority> GetPriorityProtoFromSequenceInformationValue(
    const base::Value::Dict& sequence_information) {
  const absl::optional<int> int_priority_result =
      sequence_information.FindInt("priority");
  if (int_priority_result.has_value()) {
    return Priority(int_priority_result.value());
  }

  const std::string* str_priority_result =
      sequence_information.FindString("priority");
  if (!str_priority_result) {
    LOG(ERROR) << "Field priority is missing from SequenceInformation: "
               << sequence_information;
    return absl::nullopt;
  }

  Priority priority;
  if (!Priority_Parse(*str_priority_result, &priority)) {
    LOG(ERROR) << "Unable to parse field priority in SequenceInformation: "
               << sequence_information;
    return absl::nullopt;
  }
  return priority;
}

// Returns true if `generation_guid` is required and missing.
// Returns false otherwise.
static bool IsMissingGenerationGuid(const std::string* generation_guid) {
  // Generation guid is only required for devices that aren't ChromeOS managed
  // devices.
  if (policy::ManagementServiceFactory::GetForPlatform()
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    return false;
  }
  return !generation_guid || generation_guid->empty();
}

// Returns true if any required sequence info is missing. Returns
// false otherwise.
static bool IsMissingSequenceInformation(
    const std::string* sequencing_id,
    const std::string* generation_id,
    const absl::optional<Priority> priority_result,
    const std::string* generation_guid) {
  return !sequencing_id || !generation_id || generation_id->empty() ||
         !priority_result.has_value() ||
         !Priority_IsValid(priority_result.value()) ||
         IsMissingGenerationGuid(generation_guid);
}

// Returns true if `generation_guid` can be parsed as a GUID or if
// `generation_guid` does not need to be parsed based on the type of device.
// Returns false otherwise.
static bool GenerationGuidIsValid(const std::string& generation_guid) {
  if (generation_guid.empty() &&
      policy::ManagementServiceFactory::GetForPlatform()
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    // This is a legacy ChromeOS managed device and is not required to have
    // a `generation_guid`.
    return true;
  }
  return base::Uuid::ParseCaseInsensitive(generation_guid).is_valid();
}

// Processes LOG_UPLOAD event.
void ProcessFileUpload(base::WeakPtr<FileUploadJob::Delegate> delegate,
                       Priority priority,
                       Record record_copy,
                       const ScopedReservation& scoped_reservation,
                       base::OnceCallback<void(Status)> done_cb) {
  // Here we need to determine which events we got. It would be better to
  // use protobuf reflection and detect upload_settings presence in the event,
  // but protobuf_lite library included in Chrome does not expose reflection.
  // `done_cb` called with OK causes the event to upload and then uploader picks
  // up the next event. Any other status stops the upload before the event.
  switch (record_copy.destination()) {
    case LOG_UPLOAD: {
      // Parse `record_copy.data()` string into the event.
      ::ash::reporting::LogUploadEvent log_upload_event;
      if (!log_upload_event.ParseFromArray(record_copy.data().data(),
                                           record_copy.data().size())) {
        LOG(WARNING) << "Event " << Destination_Name(record_copy.destination())
                     << " is not parseable";
        // The event is corrupt, ignore upload settings, upload with the event.
        std::move(done_cb).Run(Status::StatusOK());
        return;
      }
      if (log_upload_event.has_upload_tracker() &&
          (!log_upload_event.upload_tracker().access_parameters().empty() ||
           log_upload_event.upload_tracker().has_status())) {
        // The event already reports job as completed (succeeded or failed),
        // upload it with no further processing.
        std::move(done_cb).Run(Status::StatusOK());
        return;
      }
      // Check whether this upload is already being processed, based on the
      // whole `upload_settings` (including retry count).
      FileUploadJob::Manager::GetInstance()->Register(
          priority, std::move(record_copy), std::move(log_upload_event),
          delegate,
          base::BindOnce(
              [](ScopedReservation scoped_reservation,
                 base::OnceCallback<void(Status)> done_cb,
                 StatusOr<FileUploadJob*> job_or_error) {
                if (!job_or_error.ok()) {
                  LOG(WARNING) << "Failed to locate/create upload job, status="
                               << job_or_error.status();
                  // Upload the event as is.
                  std::move(done_cb).Run(Status::StatusOK());
                  return;
                }
                // Job has been located or created.
                job_or_error.ValueOrDie()->event_helper()->Run(
                    scoped_reservation, std::move(done_cb));
              },
              ScopedReservation(0uL, scoped_reservation), std::move(done_cb)));
      break;
    }
    default:
      LOG(WARNING) << "Event " << Destination_Name(record_copy.destination())
                   << " is not expected to have log upload settings";
      // Ignore upload settings, proceed with the event.
      std::move(done_cb).Run(Status::StatusOK());
  }
}

// Returns a tuple of <SequencingId, GenerationId> if `sequencing_id` and
// `generation_id` can be parse as numbers. Returns error status otherwise.
StatusOr<std::tuple<int64_t, int64_t>> ParseSequencingIdAndGenerationId(
    const std::string* sequencing_id,
    const std::string* generation_id) {
  int64_t seq_id;
  int64_t gen_id;

  if (!base::StringToInt64(*sequencing_id, &seq_id) ||
      !base::StringToInt64(*generation_id, &gen_id) || gen_id == 0) {
    // For backwards compatibility accept unsigned values if signed are not
    // parsed.
    // TODO(b/177677467): Remove this duplication once server is fully
    // transitioned.
    uint64_t unsigned_seq_id;
    uint64_t unsigned_gen_id;
    if (!base::StringToUint64(*sequencing_id, &unsigned_seq_id) ||
        !base::StringToUint64(*generation_id, &unsigned_gen_id) ||
        unsigned_gen_id == 0) {
      return Status(error::INVALID_ARGUMENT,
                    "Could not parse sequencing id and generation id.");
    }
    seq_id = static_cast<int64_t>(unsigned_seq_id);
    gen_id = static_cast<int64_t>(unsigned_gen_id);
  }
  return std::make_tuple(seq_id, gen_id);
}
}  // namespace

// static
StatusOr<SequenceInformation>
RecordHandlerImpl::SequenceInformationValueToProto(
    const base::Value::Dict& value) {
  const std::string* sequencing_id =
      value.FindString(UploadEncryptedReportingRequestBuilder::kSequencingId);
  const std::string* generation_id =
      value.FindString(UploadEncryptedReportingRequestBuilder::kGenerationId);
  const std::string* generation_guid =
      value.FindString(UploadEncryptedReportingRequestBuilder::kGenerationGuid);
  const auto priority_result =
      GetPriorityProtoFromSequenceInformationValue(value);
  // If required sequence info fields don't exist, or are malformed,
  // return error.
  // Note: `generation_guid` is allowed to be empty - managed devices
  // may not have it.
  if (IsMissingSequenceInformation(sequencing_id, generation_id,
                                   priority_result, generation_guid)) {
    return Status(error::INVALID_ARGUMENT,
                  base::StrCat({"Provided value lacks some fields required by "
                                "SequenceInformation proto: ",
                                value.DebugString()}));
  }

  const auto parse_seq_id_gen_id_result =
      ParseSequencingIdAndGenerationId(sequencing_id, generation_id);
  if (!parse_seq_id_gen_id_result.ok()) {
    return Status(error::INVALID_ARGUMENT,
                  base::StrCat({"Provided value did not conform to a valid "
                                "SequenceInformation proto. Invalid sequencing "
                                "id or generation id : ",
                                value.DebugString()}));
  }
  const auto [seq_id, gen_id] = parse_seq_id_gen_id_result.ValueOrDie();

  // If `generation_guid` does not exist, set it to be an empty string.
  const std::string gen_guid = generation_guid ? *generation_guid : "";
  if (!GenerationGuidIsValid(gen_guid)) {
    return Status(
        error::INVALID_ARGUMENT,
        base::StrCat({"Provided value did not conform to a valid "
                      "SequenceInformation proto. Invalid generation guid : ",
                      value.DebugString()}));
  }

  SequenceInformation proto;
  proto.set_sequencing_id(seq_id);
  proto.set_generation_id(gen_id);
  proto.set_priority(Priority(priority_result.value()));
  proto.set_generation_guid(gen_guid);
  return proto;
}

// ReportUploader handles enqueuing events on the `report_queue_`.
class RecordHandlerImpl::ReportUploader
    : public TaskRunnerContext<CompletionResponse> {
 public:
  ReportUploader(
      base::WeakPtr<FileUploadJob::Delegate> delegate,
      bool need_encryption_key,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      CompletionCallback upload_complete_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

 private:
  ~ReportUploader() override;

  void OnStart() override;
  void OnCompletion(const CompletionResponse& result) override;

  void StartUpload();
  void LogNumRecordsInUpload(size_t num_records);
  void ResumeUpload(size_t next_record);
  void FinalizeUpload();
  void OnUploadComplete(StatusOr<base::Value::Dict> response);
  void HandleFailedUpload(Status status);
  void HandleSuccessfulUpload(base::Value::Dict last_response);
  void Complete(CompletionResponse result);

  // Returns a gap record if it is necessary. Expects the contents of the
  // failedUploadedRecord field in the response:
  // {
  //   "sequencingId": 1234
  //   "generationId": 4321
  //   "priority": 3
  //   "generationGuid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
  // }
  absl::optional<EncryptedRecord> HandleFailedUploadedSequenceInformation(
      const base::Value::Dict& sequence_information);

  const base::WeakPtr<FileUploadJob::Delegate> delegate_;

  bool need_encryption_key_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<EncryptedRecord> records_ GUARDED_BY_CONTEXT(sequence_checker_);
  ScopedReservation scoped_reservation_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<UploadEncryptedReportingRequestBuilder> request_builder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Encryption key delivery callback.
  EncryptionKeyAttachedCallback encryption_key_attached_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Set for the highest record being uploaded.
  absl::optional<SequenceInformation> highest_sequence_information_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Set to |true| if force_confirm flag is present. |false| by default.
  bool force_confirm_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

RecordHandlerImpl::ReportUploader::ReportUploader(
    base::WeakPtr<FileUploadJob::Delegate> delegate,
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    CompletionCallback completion_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<CompletionResponse>(std::move(completion_cb),
                                            sequenced_task_runner),
      delegate_(delegate),
      need_encryption_key_(need_encryption_key),
      records_(std::move(records)),
      scoped_reservation_(std::move(scoped_reservation)),
      encryption_key_attached_cb_(std::move(encryption_key_attached_cb)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RecordHandlerImpl::ReportUploader::~ReportUploader() = default;

void RecordHandlerImpl::ReportUploader::OnStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (records_.empty() && !need_encryption_key_) {
    Status empty_records =
        Status(error::INVALID_ARGUMENT, "records_ was empty");
    LOG(ERROR) << empty_records;
    Complete(empty_records);
    return;
  }

  StartUpload();
}

void RecordHandlerImpl::ReportUploader::OnCompletion(
    const CompletionResponse& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // In case `OnUploadComplete` was skipped for whatever reason,
  // release the reservation (do not wait for destructor to do that).
  ScopedReservation release(std::move(scoped_reservation_));
}

void RecordHandlerImpl::ReportUploader::StartUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!records_.empty() || need_encryption_key_);

  // Log size of non-empty uploads. Key-request uploads have no records.
  if (!records_.empty()) {
    Schedule(&RecordHandlerImpl::ReportUploader::LogNumRecordsInUpload,
             base::Unretained(this), records_.size());
  }

  request_builder_ = std::make_unique<UploadEncryptedReportingRequestBuilder>(
      need_encryption_key_);
  ResumeUpload(/*next_record=*/0);
}

void RecordHandlerImpl::ReportUploader::LogNumRecordsInUpload(
    size_t num_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (policy::ManagementServiceFactory::GetForPlatform()
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    // This is a managed device, so log the upload as such.
    base::UmaHistogramCounts1000(
        "Browser.ERP.RecordsPerUploadFromManagedDevice", num_records);
  } else {
    base::UmaHistogramCounts1000(
        "Browser.ERP.RecordsPerUploadFromUnmanagedDevice", num_records);
  }
}

void RecordHandlerImpl::ReportUploader::ResumeUpload(size_t next_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (next_record < records_.size()) {
    auto& record = records_.at(next_record++);
    if (!record.has_record_copy()) {
      // Regular event, add it for upload and proceed.
      request_builder_->AddRecord(std::move(record), scoped_reservation_);
      continue;
    }
    // Asynchronously process event, add it for upload and proceed if
    // successful.
    const auto priority = record.sequence_information().priority();
    auto record_copy = std::move(*record.mutable_record_copy());
    record.clear_record_copy();
    auto resume_cb = base::BindPostTaskToCurrentDefault(base::BindOnce(
        [](RecordHandlerImpl::ReportUploader* self, EncryptedRecord record,
           size_t next_record, Status processed_status) {
          if (!processed_status.ok()) {
            // Event not processed, stop before it.
            // Do not add the current event and any later ones.
            self->FinalizeUpload();
            return;
          }
          // Event processed (next upload tracking event posted, if needed),
          // add current event to upload (`record_copy` has been removed
          // from it) and proceed.
          DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
          self->request_builder_->AddRecord(std::move(record),
                                            self->scoped_reservation_);
          self->ResumeUpload(next_record);  // Already advanced!
        },
        base::Unretained(this),  // `ReportUploader` destructs on completion.
        std::move(record), next_record));
    FileUploadJob::Manager::GetInstance()->sequenced_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ProcessFileUpload, delegate_, priority,
                                  std::move(record_copy),
                                  ScopedReservation(0uL, scoped_reservation_),
                                  std::move(resume_cb)));
    return;  // We will resume on `resume_cb`
  }

  FinalizeUpload();
}

void RecordHandlerImpl::ReportUploader::FinalizeUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Records have been captured in the request, safe to clear the vector.
  records_.clear();

  // Assign random UUID as the request id for server side log correlation
  const auto request_id = base::Token::CreateRandom().ToString();
  request_builder_->SetRequestId(request_id);

  auto request_result = request_builder_->Build();
  request_builder_.reset();
  if (!request_result.has_value()) {
    HandleFailedUpload(
        Status(error::FAILED_PRECONDITION, "Failure to build request"));
    return;
  }

  auto response_cb = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&RecordHandlerImpl::ReportUploader::OnUploadComplete,
                     base::Unretained(this)));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::Value::Dict request,
             ReportingServerConnector::ResponseCallback response_cb) {
            ReportingServerConnector::UploadEncryptedReport(
                std::move(request), std::move(response_cb));
          },
          std::move(request_result.value()), std::move(response_cb)));
}

void RecordHandlerImpl::ReportUploader::OnUploadComplete(
    StatusOr<base::Value::Dict> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Release reservation right away, since we no londer need to keep
  // `base::Value::Dict request` it was referring to.
  scoped_reservation_.Reduce(0uL);

  if (!response.ok()) {
    HandleFailedUpload(response.status());
    return;
  }

  HandleSuccessfulUpload(std::move(response.ValueOrDie()));
}

void RecordHandlerImpl::ReportUploader::HandleFailedUpload(Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (highest_sequence_information_.has_value()) {
    Complete(SuccessfulUploadResponse{
        .sequence_information =
            std::move(highest_sequence_information_.value()),
        .force_confirm = force_confirm_});
    return;
  }

  Complete(status);
}

void RecordHandlerImpl::ReportUploader::HandleSuccessfulUpload(
    base::Value::Dict last_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // {{{Note}}} ERP Response Payload Overview
  //
  //  {
  //    "lastSucceedUploadedRecord": ... // SequenceInformation proto
  //    "firstFailedUploadedRecord": {
  //      "failedUploadedRecord": ... // SequenceInformation proto
  //      "failureStatus": ... // Status proto
  //    },
  //    "encryptionSettings": ... // EncryptionSettings proto
  //    "forceConfirm": true, // if present, flag that lastSucceedUploadedRecord
  //                          // is to be accepted unconditionally by client
  //    // Internal control
  //    "enableUploadSizeAdjustment": true,  // If present, upload size
  //                                         // adjustment is enabled.
  //  }
  const base::Value::Dict* last_succeed_uploaded_record =
      last_response.FindDict("lastSucceedUploadedRecord");
  if (last_succeed_uploaded_record != nullptr) {
    auto seq_info_result =
        SequenceInformationValueToProto(*last_succeed_uploaded_record);
    if (seq_info_result.ok()) {
      highest_sequence_information_ = std::move(seq_info_result.ValueOrDie());
    } else {
      LOG(ERROR) << "Server responded with an invalid SequenceInformation "
                    "for lastSucceedUploadedRecord"
                 << "\n"
                 << "error status = " << seq_info_result.status() << "\n"
                 << "last_succeed_uploaded_record = "
                 << *last_succeed_uploaded_record;
    }
  }

  // Handle forceConfirm flag, if present.
  const auto force_confirm_flag = last_response.FindBool("forceConfirm");
  if (force_confirm_flag.has_value() && force_confirm_flag.value()) {
    force_confirm_ = true;
  }

  // Handle enableUploadSizeAdjustment flag, if present.
  const auto enable_upload_size_adjustment =
      last_response.FindBool("enableUploadSizeAdjustment");
  if (enable_upload_size_adjustment.has_value()) {
    EventUploadSizeController::Enabler::Set(
        enable_upload_size_adjustment.value());
  }

  // Handle the encryption settings.
  // Note: server can attach it to response regardless of whether
  // the response indicates success or failure, and whether the client
  // set attach_encryption_settings to true in request.
  const base::Value::Dict* signed_encryption_key_record =
      last_response.FindDict("encryptionSettings");
  if (signed_encryption_key_record != nullptr) {
    const std::string* public_key_str =
        signed_encryption_key_record->FindString("publicKey");
    const auto public_key_id_result =
        signed_encryption_key_record->FindInt("publicKeyId");
    const std::string* public_key_signature_str =
        signed_encryption_key_record->FindString("publicKeySignature");
    std::string public_key;
    std::string public_key_signature;
    if (public_key_str != nullptr &&
        base::Base64Decode(*public_key_str, &public_key) &&
        public_key_signature_str != nullptr &&
        base::Base64Decode(*public_key_signature_str, &public_key_signature) &&
        public_key_id_result.has_value()) {
      SignedEncryptionInfo signed_encryption_key;
      signed_encryption_key.set_public_asymmetric_key(public_key);
      signed_encryption_key.set_public_key_id(public_key_id_result.value());
      signed_encryption_key.set_signature(public_key_signature);
      std::move(encryption_key_attached_cb_).Run(signed_encryption_key);
      need_encryption_key_ = false;
    }
  }

  // Check if a record was unprocessable on the server.
  const base::Value::Dict* failed_uploaded_record =
      last_response.FindDictByDottedPath(
          "firstFailedUploadedRecord.failedUploadedRecord");
  if (!force_confirm_ && failed_uploaded_record != nullptr) {
    // The record we uploaded previously was unprocessable by the server, if
    // the record was after the current |highest_sequence_information_| we
    // should return a gap record. A gap record consists of an EncryptedRecord
    // with just SequenceInformation. The server will report success for the
    // gap record and |highest_sequence_information_| will be updated in the
    // next response. In the future there may be recoverable |failureStatus|,
    // but for now all the device can do is delete the record.
    auto gap_record_result =
        HandleFailedUploadedSequenceInformation(*failed_uploaded_record);
    if (gap_record_result.has_value()) {
      LOG(ERROR) << "Data Loss. Record was unprocessable by the server: "
                 << *failed_uploaded_record;
      records_.push_back(std::move(gap_record_result.value()));
    }
  }

  if (!records_.empty()) {
    // Upload the next record but do not request encryption key again.
    StartUpload();
    return;
  }

  // No more records to process. Return the highest_sequence_information_ if
  // available.
  if (highest_sequence_information_.has_value()) {
    Complete(SuccessfulUploadResponse{
        .sequence_information =
            std::move(highest_sequence_information_.value()),
        .force_confirm = force_confirm_});
    return;
  }

  Complete(Status(error::INTERNAL, "Unable to upload any records"));
}

absl::optional<EncryptedRecord>
RecordHandlerImpl::ReportUploader::HandleFailedUploadedSequenceInformation(
    const base::Value::Dict& sequence_information) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!highest_sequence_information_.has_value()) {
    LOG(ERROR) << "highest_sequence_information_ has no value.";
    return absl::nullopt;
  }

  auto seq_info_result = SequenceInformationValueToProto(sequence_information);
  if (!seq_info_result.ok()) {
    LOG(ERROR) << "Server responded with an invalid SequenceInformation for "
                  "firstFailedUploadedRecord.failedUploadedRecord:"
               << sequence_information;
    return absl::nullopt;
  }

  SequenceInformation& seq_info = seq_info_result.ValueOrDie();

  // |seq_info| should be of the same generation, generation guid, and priority
  // as highest_sequence_information_, and have the next sequencing_id.
  if (seq_info.generation_id() !=
          highest_sequence_information_->generation_id() ||
      seq_info.generation_guid() !=
          highest_sequence_information_->generation_guid() ||
      seq_info.priority() != highest_sequence_information_->priority() ||
      seq_info.sequencing_id() !=
          highest_sequence_information_->sequencing_id() + 1) {
    LOG(ERROR) << "Sequence info fields are incorrect.";
    return absl::nullopt;
  }

  // Build a gap record and return it.
  EncryptedRecord encrypted_record;
  *encrypted_record.mutable_sequence_information() = std::move(seq_info);
  return encrypted_record;
}

void RecordHandlerImpl::ReportUploader::Complete(
    CompletionResponse completion_result) {
  Schedule(&RecordHandlerImpl::ReportUploader::Response, base::Unretained(this),
           completion_result);
}

RecordHandlerImpl::RecordHandlerImpl(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    std::unique_ptr<FileUploadJob::Delegate> delegate)
    : sequenced_task_runner_(sequenced_task_runner),
      delegate_(std::move(delegate)) {}

RecordHandlerImpl::~RecordHandlerImpl() {
  FileUploadJob::Manager::GetInstance()->sequenced_task_runner()->DeleteSoon(
      FROM_HERE, std::move(delegate_));
}

void RecordHandlerImpl::HandleRecords(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    CompletionCallback upload_complete_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb) {
  // Prepare weak pointer to delegate for ChromeOS Ash case only, since
  // file uploads are not available in other configurations: `delegate_` is
  // nullptr there, and so is the weak pointer.
  base::WeakPtr<FileUploadJob::Delegate> delegate;
  if (delegate_.get()) {
    delegate = delegate_->GetWeakPtr();
  }
  Start<RecordHandlerImpl::ReportUploader>(
      delegate, need_encryption_key, std::move(records),
      std::move(scoped_reservation), std::move(upload_complete_cb),
      std::move(encryption_key_attached_cb), sequenced_task_runner_);
}
}  // namespace reporting
