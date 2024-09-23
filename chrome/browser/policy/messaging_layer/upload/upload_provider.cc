// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"

#include <list>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/upload/configuration_file_controller.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/util/backoff_settings.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "net/base/backoff_entry.h"

namespace reporting {

namespace {
std::unique_ptr<ConfigurationFileController> CreateConfigurationFileController(
    UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ConfigurationFileController>(
      update_config_in_missive_cb);
#else   // !BUILDFLAG(IS_CHROMEOS)
  return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
}
}  // namespace

// EncryptedReportingUploadProvider refcounted helper class.
class EncryptedReportingUploadProvider::UploadHelper
    : public base::RefCountedDeleteOnSequence<UploadHelper> {
 public:
  UploadHelper(ReportSuccessfulUploadCallback report_successful_upload_cb,
               EncryptionKeyAttachedCallback encryption_key_attached_cb,
               UpdateConfigInMissiveCallback update_config_in_missive_cb,
               UploadClientBuilderCb upload_client_builder_cb,
               scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);
  UploadHelper(const UploadHelper& other) = delete;
  UploadHelper& operator=(const UploadHelper& other) = delete;

  // Requests new cloud policy client (can be invoked on any thread)
  void PostNewUploadClientRequest();

  // Uploads encrypted records (can be invoked on any thread).
  void EnqueueUpload(bool need_encryption_key,
                     std::vector<EncryptedRecord> records,
                     ScopedReservation scoped_reservation,
                     UploadEnqueuedCallback enqueued_cb) const;

 private:
  friend class base::RefCountedDeleteOnSequence<UploadHelper>;
  friend class base::DeleteHelper<UploadHelper>;

  // Refcounted object can only have private or protected destructor.
  ~UploadHelper();

  // Stages of upload client creation, scheduled on a sequenced task runner.
  void TryNewUploadClientRequest();
  void OnUploadClientResult(
      StatusOr<std::unique_ptr<UploadClient>> client_result);

  // Helpers to send the configuration file gotten from the response
  // to the `ConfigurationFileController`.
  void UpdateConfigFile(ConfigFile file);

  // Uploads encrypted records on sequenced task runner (and thus capable of
  // detecting whether upload client is ready or not). If not ready,
  // it will wait and then upload.
  void EnqueueUploadInternal(bool need_encryption_key,
                             std::vector<EncryptedRecord> records,
                             ScopedReservation scoped_reservation,
                             UploadEnqueuedCallback enqueued_cb);

  // Sequence task runner and checker used during
  // `PostNewUploadClientRequest` processing.
  // It is also used to protect `upload_client_`.
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequenced_task_checker_);

  // Callbacks for successful upload, key delivery and configuration file
  // attached.
  const ReportSuccessfulUploadCallback report_successful_upload_cb_;
  const EncryptionKeyAttachedCallback encryption_key_attached_cb_;
  ConfigFileAttachedCallback config_file_attached_cb_;

  // Callback for upload client creation.
  UploadClientBuilderCb upload_client_builder_cb_;

  // Tracking of asynchronous stages.
  std::atomic<bool> upload_client_request_in_progress_{false};
  const std::unique_ptr<::net::BackoffEntry> backoff_entry_;

  // Stored data from upload requests before upload client is ready.
  // Note that vectors of records submitted for upload are mapped by
  // generation id (which is the same for all records being uploaded at once
  // since they originate from the same priority queue). As a result, if
  // the caller attempts to upload the same records multiple times (e.g.
  // because it did not yet get a confirmation from server), we will only
  // hold to one set of records.
  // |stored_reservations_| reflect amount of memory assigned to the respective
  // element in |stored_records_| (if it is assigned, otherwise it is absent
  // from the map). unique_ptr is used, to enable moving/erasing the map
  // elements. Guarded by sequenced_task_runner_.
  base::flat_map</*generation_id*/ int64_t, std::vector<EncryptedRecord>>
      stored_records_ GUARDED_BY_CONTEXT(sequenced_task_checker_);
  base::flat_map</*generation_id*/ int64_t, std::unique_ptr<ScopedReservation>>
      stored_reservations_ GUARDED_BY_CONTEXT(sequenced_task_checker_);
  bool stored_need_encryption_key_ GUARDED_BY_CONTEXT(sequenced_task_checker_){
      false};
  int stored_config_file_version_ GUARDED_BY_CONTEXT(sequenced_task_checker_){
      0};

  // Upload client (protected by sequenced task runner). Once set, is used
  // repeatedly.
  std::unique_ptr<UploadClient> upload_client_
      GUARDED_BY_CONTEXT(sequenced_task_checker_);

  // Configuration file controller (protected by sequenced task runner). Once
  // set, is used repeatedly.
  std::unique_ptr<ConfigurationFileController> config_controller_
      GUARDED_BY_CONTEXT(sequenced_task_checker_);

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<UploadHelper> weak_ptr_factory_{this};
};

EncryptedReportingUploadProvider::UploadHelper::UploadHelper(
    ReportSuccessfulUploadCallback report_successful_upload_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    UpdateConfigInMissiveCallback update_config_in_missive_cb,
    UploadClientBuilderCb upload_client_builder_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : base::RefCountedDeleteOnSequence<UploadHelper>(sequenced_task_runner),
      sequenced_task_runner_(std::move(sequenced_task_runner)),
      report_successful_upload_cb_(report_successful_upload_cb),
      encryption_key_attached_cb_(encryption_key_attached_cb),
      upload_client_builder_cb_(std::move(upload_client_builder_cb)),
      backoff_entry_(GetBackoffEntry()),
      config_controller_(CreateConfigurationFileController(
          std::move(update_config_in_missive_cb))) {
  config_file_attached_cb_ =
      base::BindPostTask(sequenced_task_runner_,
                         base::BindRepeating(&UploadHelper::UpdateConfigFile,
                                             weak_ptr_factory_.GetWeakPtr()));
  DETACH_FROM_SEQUENCE(sequenced_task_checker_);
}

EncryptedReportingUploadProvider::UploadHelper::~UploadHelper() {
  // Weak pointer factory must be destructed on the sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
}

void EncryptedReportingUploadProvider::UploadHelper::
    PostNewUploadClientRequest() {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UploadHelper::TryNewUploadClientRequest,
                                weak_ptr_factory_.GetWeakPtr()));
}

void EncryptedReportingUploadProvider::UploadHelper::
    TryNewUploadClientRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (upload_client_ != nullptr) {
    return;
  }
  if (upload_client_request_in_progress_) {
    return;
  }
  upload_client_request_in_progress_ = true;

  sequenced_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<UploadHelper> self) {
            if (!self) {
              return;  // Provider expired
            }
            std::move(self->upload_client_builder_cb_)
                .Run(base::BindPostTask(
                    self->sequenced_task_runner_,
                    base::BindRepeating(&UploadHelper::OnUploadClientResult,
                                        self)));
          },
          weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_->GetTimeUntilRelease());

  // Increase backoff_entry_ for next request.
  backoff_entry_->InformOfRequest(/*succeeded=*/false);
}

void EncryptedReportingUploadProvider::UploadHelper::OnUploadClientResult(
    StatusOr<std::unique_ptr<UploadClient>> client_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (!client_result.has_value()) {
    upload_client_request_in_progress_ = false;
    PostNewUploadClientRequest();
    return;
  }

  // Record upload client to be used.
  upload_client_ = std::move(client_result.value());
  backoff_entry_->InformOfRequest(/*succeeded=*/true);
  upload_client_request_in_progress_ = false;

  // Upload client is ready, upload all previously stored requests (if any).
  while (!stored_records_.empty() || stored_need_encryption_key_) {
    std::vector<EncryptedRecord> records;
    ScopedReservation scoped_reservation;
    if (!stored_records_.empty()) {
      records = std::move(stored_records_.begin()->second);
      auto it = stored_reservations_.find(stored_records_.begin()->first);
      if (it != stored_reservations_.end()) {
        scoped_reservation.HandOver(*it->second);
        CHECK(!it->second->reserved());
        stored_reservations_.erase(it);
      }
      stored_records_.erase(stored_records_.begin());
    }
    const bool need_encryption_key =
        std::exchange(stored_need_encryption_key_, false);
    upload_client_->EnqueueUpload(
        need_encryption_key, stored_config_file_version_, std::move(records),
        std::move(scoped_reservation),
        base::BindOnce([](StatusOr<std::list<int64_t>> cached_records_seq_ids) {
          LOG_IF(ERROR, !cached_records_seq_ids.has_value())
              << "Upload not enqueued, error="
              << cached_records_seq_ids.error();
        }),
        report_successful_upload_cb_, encryption_key_attached_cb_,
        config_file_attached_cb_);
  }
}

void EncryptedReportingUploadProvider::UploadHelper::UpdateConfigFile(
    ConfigFile file) {
  // This function is only called in ChromeOS so no nullptr check is needed.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  stored_config_file_version_ =
      config_controller_->HandleConfigurationFile(std::move(file));
}

void EncryptedReportingUploadProvider::UploadHelper::EnqueueUpload(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    UploadEnqueuedCallback enqueued_cb) const {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UploadHelper::EnqueueUploadInternal,
          weak_ptr_factory_.GetMutableWeakPtr(), need_encryption_key,
          std::move(records), std::move(scoped_reservation),
          Scoped<StatusOr<std::list<int64_t>>>(
              std::move(enqueued_cb),
              base::unexpected(Status(error::UNAVAILABLE,
                                      "UploadHelper has been destructed")))));
}

// static
void EncryptedReportingUploadProvider::UploadHelper::EnqueueUploadInternal(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    UploadEnqueuedCallback enqueued_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (upload_client_ == nullptr) {
    stored_need_encryption_key_ |= need_encryption_key;
    int64_t generation_id = 0;
    if (!records.empty() && records.begin()->has_sequence_information() &&
        records.begin()->sequence_information().has_generation_id()) {
      generation_id = records.begin()->sequence_information().generation_id();
    }
    std::list<int64_t> cached_records_seq_ids;
    for (const auto& record : records) {
      cached_records_seq_ids.push_back(
          record.sequence_information().sequencing_id());
    }
    stored_records_.emplace(generation_id, std::move(records));
    stored_reservations_.emplace(
        generation_id,
        std::make_unique<ScopedReservation>(std::move(scoped_reservation)));
    // Report success even though the upload has not been executed.
    // All stored records are listed as cached, because we don't need to re-send
    // them (although actual cache will be created later).
    // And actual upload success is reported through two permanent repeating
    // callbacks, once the client is created and upload is executed.
    std::move(enqueued_cb).Run(std::move(cached_records_seq_ids));
    return;
  }
  upload_client_->EnqueueUpload(
      need_encryption_key, stored_config_file_version_, std::move(records),
      std::move(scoped_reservation), std::move(enqueued_cb),
      report_successful_upload_cb_, encryption_key_attached_cb_,
      config_file_attached_cb_);
}

// EncryptedReportingUploadProvider implementation.

EncryptedReportingUploadProvider::EncryptedReportingUploadProvider(
    ReportSuccessfulUploadCallback report_successful_upload_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    UpdateConfigInMissiveCallback update_config_in_missive_cb,
    UploadClientBuilderCb upload_client_builder_cb)
    : helper_(base::MakeRefCounted<UploadHelper>(
          report_successful_upload_cb,
          encryption_key_attached_cb,
          update_config_in_missive_cb,
          std::move(upload_client_builder_cb),
          base::SequencedTaskRunner::GetCurrentDefault())) {
  helper_->PostNewUploadClientRequest();
}

EncryptedReportingUploadProvider::~EncryptedReportingUploadProvider() = default;

void EncryptedReportingUploadProvider::RequestUploadEncryptedRecords(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    UploadEnqueuedCallback enqueued_cb) {
  CHECK(helper_);
  helper_->EnqueueUpload(need_encryption_key, std::move(records),
                         std::move(scoped_reservation), std::move(enqueued_cb));
}

base::WeakPtr<EncryptedReportingUploadProvider>
EncryptedReportingUploadProvider::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
EncryptedReportingUploadProvider::UploadClientBuilderCb
EncryptedReportingUploadProvider::GetUploadClientBuilder() {
  return base::BindOnce(&UploadClient::Create);
}
}  // namespace reporting
