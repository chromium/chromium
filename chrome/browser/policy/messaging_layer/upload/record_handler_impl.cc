// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/upload/server_uploader.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {
namespace {

// Processes LOG_UPLOAD event.
void ProcessFileUpload(
    Priority priority,
    Record record_copy,
    const ScopedReservation& scoped_reservation,
    base::RepeatingCallback<FileUploadJob::Delegate::SmartPtr()>
        delegate_factory,
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
          delegate_factory.Run(),
          base::BindOnce(
              [](ScopedReservation scoped_reservation,
                 base::OnceCallback<void(Status)> done_cb,
                 StatusOr<FileUploadJob*> job_or_error) {
                if (!job_or_error.has_value()) {
                  LOG(WARNING) << "Failed to locate/create upload job, status="
                               << job_or_error.error();
                  // Upload the event as is.
                  std::move(done_cb).Run(Status::StatusOK());
                  return;
                }
                // Job has been located or created.
                job_or_error.value()->event_helper()->Run(scoped_reservation,
                                                          std::move(done_cb));
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
}  // namespace

// ReportUploader handles enqueuing events on the `report_queue_`.
class RecordHandlerImpl::ReportUploader
    : public TaskRunnerContext<CompletionResponse> {
 public:
  ReportUploader(
      bool need_encryption_key,
      int config_file_version,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      base::RepeatingCallback<FileUploadJob::Delegate::SmartPtr()>
          delegate_factory,
      UploadEnqueuedCallback enqueued_cb,
      CompletionCallback upload_complete_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      ConfigFileAttachedCallback config_file_attached_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

 private:
  ~ReportUploader() override;

  void OnStart() override;
  void OnCompletion(const CompletionResponse& result) override;

  void StartUpload();
  void ResumeUpload(size_t next_record);
  void UploadRequest(size_t next_record);
  void OnUploadComplete(StatusOr<UploadResponseParser> response_result);
  void Complete(CompletionResponse result);

  bool need_encryption_key_ GUARDED_BY_CONTEXT(sequence_checker_);
  int config_file_version_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<EncryptedRecord> records_ GUARDED_BY_CONTEXT(sequence_checker_);
  ScopedReservation scoped_reservation_ GUARDED_BY_CONTEXT(sequence_checker_);
  UploadEnqueuedCallback enqueued_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  // File upload delegate factory.
  const base::RepeatingCallback<FileUploadJob::Delegate::SmartPtr()>
      delegate_factory_;

  // Encryption key delivery callback.
  EncryptionKeyAttachedCallback encryption_key_attached_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Configuration file delivery callback.
  ConfigFileAttachedCallback config_file_attached_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Set to |true| if force_confirm flag is present. |false| by default.
  bool force_confirm_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

RecordHandlerImpl::ReportUploader::ReportUploader(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    base::RepeatingCallback<FileUploadJob::Delegate::SmartPtr()>
        delegate_factory,
    UploadEnqueuedCallback enqueued_cb,
    CompletionCallback completion_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    ConfigFileAttachedCallback config_file_attached_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<CompletionResponse>(std::move(completion_cb),
                                            sequenced_task_runner),
      need_encryption_key_(need_encryption_key),
      config_file_version_(config_file_version),
      records_(std::move(records)),
      scoped_reservation_(std::move(scoped_reservation)),
      enqueued_cb_(std::move(enqueued_cb)),
      delegate_factory_(delegate_factory),
      encryption_key_attached_cb_(std::move(encryption_key_attached_cb)),
      config_file_attached_cb_(std::move(config_file_attached_cb)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RecordHandlerImpl::ReportUploader::~ReportUploader() = default;

void RecordHandlerImpl::ReportUploader::OnStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (records_.empty() && !need_encryption_key_) {
    Status empty_records =
        Status(error::INVALID_ARGUMENT, "records_ was empty");
    LOG(ERROR) << empty_records;
    if (enqueued_cb_) {
      std::move(enqueued_cb_).Run(base::unexpected(empty_records));
    }
    Complete(base::unexpected(std::move(empty_records)));
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

  ResumeUpload(/*next_record=*/0);
}

void RecordHandlerImpl::ReportUploader::ResumeUpload(size_t next_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (next_record < records_.size()) {
    auto& record = records_.at(next_record++);
    if (!record.has_record_copy()) {
      // Regular event, add it for upload and proceed.
      continue;
    }
    // Asynchronously process event, add it for upload and proceed if
    // successful.
    const auto priority = record.sequence_information().priority();
    auto record_copy = std::move(*record.mutable_record_copy());
    record.clear_record_copy();
    auto resume_cb = base::BindPostTaskToCurrentDefault(base::BindOnce(
        [](RecordHandlerImpl::ReportUploader* self, size_t next_record,
           Status processed_status) {
          if (!processed_status.ok()) {
            // Event not processed, stop before it.
            // Do not add the current event and any later ones.
            self->UploadRequest(next_record);
            return;
          }
          // Event processed (next upload tracking event posted, if needed),
          // add current event to upload (`record_copy` has been removed
          // from it) and proceed.
          self->ResumeUpload(next_record);  // Already advanced!
        },
        base::Unretained(this),  // `ReportUploader` destructs on completion.
        next_record));
    ProcessFileUpload(priority, std::move(record_copy),
                      ScopedReservation(0uL, scoped_reservation_),
                      delegate_factory_, std::move(resume_cb));
    return;  // We will resume on `resume_cb`
  }

  UploadRequest(next_record);
}

void RecordHandlerImpl::ReportUploader::UploadRequest(size_t next_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_LE(next_record, records_.size());
  // Release records beyond `next_record`.
  records_.erase(records_.begin() + next_record, records_.end());

  // Upload selected records on UI.
  auto response_cb = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RecordHandlerImpl::ReportUploader::OnUploadComplete,
                     base::Unretained(this)));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     need_encryption_key_, config_file_version_,
                     std::move(records_), std::move(scoped_reservation_),
                     std::move(enqueued_cb_), std::move(response_cb)));
}

void RecordHandlerImpl::ReportUploader::OnUploadComplete(
    StatusOr<UploadResponseParser> response_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(records_.empty()) << "All records have to had been processed";
  // Release reservation right away, since we no londer need to keep
  // `base::Value::Dict request` it was referring to.
  scoped_reservation_.Reduce(0uL);

  if (!response_result.has_value()) {
    Complete(base::unexpected(response_result.error()));
    return;
  }
  const auto& response_parser = response_result.value();

  // Handle forceConfirm flag, if present.
  const auto force_confirm_flag = response_parser.force_confirm_flag();

  // Handle enableUploadSizeAdjustment flag, if present.
  EventUploadSizeController::Enabler::Set(
      response_parser.enable_upload_size_adjustment());

  // Handle the encryption settings.
  // Note: server can attach it to response regardless of whether
  // the response indicates success or failure, and whether the client
  // set attach_encryption_settings to true in request.
  auto encryption_settings_result = response_parser.encryption_settings();
  if (encryption_settings_result.has_value()) {
    std::move(encryption_key_attached_cb_)
        .Run(std::move(encryption_settings_result.value()));
    need_encryption_key_ = false;
  }

  // Handle the configuration file.
  // The server attaches the configuration file if it was requested
  // by the client. Adding a check to make sure to only process it if the
  // feature is enabled on the client side.
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(kShouldRequestConfigurationFile)) {
    auto config_file_result = response_parser.config_file();
    if (config_file_result.has_value()) {
      config_file_attached_cb_.Run(std::move(config_file_result.value()));
    } else {
      base::UmaHistogramEnumeration("Browser.ERP.ConfigFileParsingError",
                                    config_file_result.error().code(),
                                    error::Code::MAX_VALUE);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Check if a record was unprocessable on the server.
  StatusOr<EncryptedRecord> failed_uploaded_record =
      response_parser.gap_record_for_permanent_failure();
  if (failed_uploaded_record.has_value()) {
    // Do not request encryption key again, even if the original failed - the
    // key should still been delivered.
    need_encryption_key_ = false;
    StartUpload();
    return;
  }

  // If failed upload is returned but is not parseable or does not match the
  // successfully uploaded part, just log an error.
  LOG_IF(ERROR, failed_uploaded_record.error().code() != error::NOT_FOUND)
      << failed_uploaded_record.error();

  // If nothing was uploaded successfully, return error.
  auto last_successfully_uploaded_record_sequence_info =
      response_parser.last_successfully_uploaded_record_sequence_info();
  if (!last_successfully_uploaded_record_sequence_info.has_value()) {
    LOG(ERROR) << last_successfully_uploaded_record_sequence_info.error();
    Complete(base::unexpected(
        Status(error::INTERNAL, "Unable to upload any records")));
    return;
  }

  // Generate and return the highest_sequence_information.
  Complete(SuccessfulUploadResponse{
      .sequence_information =
          std::move(last_successfully_uploaded_record_sequence_info.value()),
      .force_confirm = force_confirm_flag});
}

void RecordHandlerImpl::ReportUploader::Complete(
    CompletionResponse completion_result) {
  Schedule(&RecordHandlerImpl::ReportUploader::Response, base::Unretained(this),
           completion_result);
}

RecordHandlerImpl::RecordHandlerImpl(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    base::RepeatingCallback<FileUploadJob::Delegate::SmartPtr()>
        delegate_factory)
    : sequenced_task_runner_(sequenced_task_runner),
      delegate_factory_(delegate_factory) {}

RecordHandlerImpl::~RecordHandlerImpl() = default;

void RecordHandlerImpl::HandleRecords(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    UploadEnqueuedCallback enqueued_cb,
    CompletionCallback upload_complete_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    ConfigFileAttachedCallback config_file_attached_cb) {
  Start<RecordHandlerImpl::ReportUploader>(
      need_encryption_key, config_file_version, std::move(records),
      std::move(scoped_reservation), delegate_factory_, std::move(enqueued_cb),
      std::move(upload_complete_cb), std::move(encryption_key_attached_cb),
      std::move(config_file_attached_cb), sequenced_task_runner_);
}
}  // namespace reporting
