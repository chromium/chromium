// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/storage_selector/storage_selector.h"
#include "chrome/browser/policy/messaging_layer/util/dm_token_retriever_provider.h"
#include "chrome/common/chrome_paths.h"
#include "components/reporting/client/dm_token_retriever.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#include "components/reporting/encryption/verification.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace reporting {

#if !BUILDFLAG(IS_CHROMEOS)
namespace {

const base::FilePath::CharType kReportingDirectory[] =
    FILE_PATH_LITERAL("reporting");

}  // namespace
#endif  // !BUILDFLAG(IS_CHROMEOS)

ReportingClient::ReportingClient()
    : ReportQueueProvider(
          base::BindRepeating(
              [](base::OnceCallback<void(
                     StatusOr<scoped_refptr<StorageModuleInterface>>)>
                     storage_created_cb) {
#if BUILDFLAG(IS_CHROMEOS)
                StorageSelector::CreateMissiveStorageModule(
                    std::move(storage_created_cb));
#else   // !BUILDFLAG(IS_CHROMEOS)
                base::FilePath reporting_path;
                const auto res = base::PathService::Get(chrome::DIR_USER_DATA,
                                                        &reporting_path);
                CHECK(res) << "Could not retrieve base path";
                reporting_path = reporting_path.Append(kReportingDirectory);
                StorageSelector::CreateLocalStorageModule(
                    reporting_path, SignatureVerifier::VerificationKey(),
                    CompressionInformation::COMPRESSION_SNAPPY,
                    base::BindRepeating(&ReportingClient::AsyncStartUploader),
                    std::move(storage_created_cb));
#endif  // !BUILDFLAG(IS_CHROMEOS)
              }),
          base::SequencedTaskRunner::GetCurrentDefault()) {
}

ReportingClient::~ReportingClient() = default;

// static
ReportingClient* ReportingClient::GetInstance() {
  return base::Singleton<ReportingClient>::get();
}

// static
ReportQueueProvider* ReportQueueProvider::GetInstance() {
  // Forward to ReportingClient::GetInstance, because
  // base::Singleton<ReportingClient>::get() cannot be called
  // outside ReportingClient class.
  return ReportingClient::GetInstance();
}

void ReportingClient::ConfigureReportQueue(
    std::unique_ptr<ReportQueueConfiguration> configuration,
    ReportQueueProvider::ReportQueueConfiguredCallback completion_cb) {
  // If DM token has already been set (only likely for testing purposes or until
  // pre-existing events are migrated over to use event types instead), we do
  // nothing and trigger completion callback with report queue config.
  if (!configuration->dm_token().empty()) {
    std::move(completion_cb).Run(std::move(configuration));
    return;
  }

  auto dm_token_retriever_provider =
      std::make_unique<DMTokenRetrieverProvider>();
  auto dm_token_retriever =
      std::move(dm_token_retriever_provider)
          ->GetDMTokenRetrieverForEventType(configuration->event_type());

  // Trigger completion callback with an internal error if no DM token retriever
  // found
  if (!dm_token_retriever) {
    std::move(completion_cb)
        .Run(base::unexpected(
            Status(error::INTERNAL,
                   base::StrCat({"No DM token retriever found for event type=",
                                 base::NumberToString(static_cast<int>(
                                     configuration->event_type()))}))));
    return;
  }

  std::move(dm_token_retriever)
      ->RetrieveDMToken(base::BindOnce(
          [](std::unique_ptr<ReportQueueConfiguration> configuration,
             ReportQueueProvider::ReportQueueConfiguredCallback completion_cb,
             StatusOr<std::string> dm_token_result) {
            // Trigger completion callback with error if there was an error
            // retrieving DM token.
            if (!dm_token_result.has_value()) {
              std::move(completion_cb)
                  .Run(base::unexpected(dm_token_result.error()));
              return;
            }

            // Set DM token in config and trigger completion callback with the
            // corresponding result.
            auto config_result =
                configuration->SetDMToken(dm_token_result.value());

            // Fail on error
            if (!config_result.ok()) {
              std::move(completion_cb).Run(base::unexpected(config_result));
              return;
            }

            // Success, run completion callback with updated config
            std::move(completion_cb).Run(std::move(configuration));
          },
          std::move(configuration), std::move(completion_cb)));
}

#if !BUILDFLAG(IS_CHROMEOS)
// Uploader is passed to Storage in order to upload messages using
// `UploadClient`.
class ReportingClient::Uploader : public UploaderInterface {
 public:
  using UploadCallback = base::OnceCallback<
      Status(bool, std::vector<EncryptedRecord>, ScopedReservation)>;

  static std::unique_ptr<Uploader> Create(bool need_encryption_key,
                                          UploadCallback upload_callback) {
    return base::WrapUnique(
        new Uploader(need_encryption_key, std::move(upload_callback)));
  }

  ~Uploader() override = default;
  Uploader(const Uploader& other) = delete;
  Uploader& operator=(const Uploader& other) = delete;

  void ProcessRecord(EncryptedRecord data,
                     ScopedReservation scoped_reservation,
                     base::OnceCallback<void(bool)> processed_cb) override {
    helper_.AsyncCall(&Helper::ProcessRecord)
        .WithArgs(std::move(data), std::move(scoped_reservation),
                  std::move(processed_cb));
  }
  void ProcessGap(SequenceInformation start,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override {
    helper_.AsyncCall(&Helper::ProcessGap)
        .WithArgs(std::move(start), count, std::move(processed_cb));
  }

  void Completed(Status final_status) override {
    helper_.AsyncCall(&Helper::Completed).WithArgs(final_status);
  }

 private:
  // Helper class that performs actions, wrapped in SequenceBound by |Uploader|.
  class Helper {
   public:
    Helper(bool need_encryption_key, UploadCallback upload_callback);
    Helper(const Helper& other) = delete;
    Helper& operator=(const Helper& other) = delete;
    void ProcessRecord(EncryptedRecord data,
                       ScopedReservation scoped_reservation,
                       base::OnceCallback<void(bool)> processed_cb);
    void ProcessGap(SequenceInformation start,
                    uint64_t count,
                    base::OnceCallback<void(bool)> processed_cb);
    void Completed(Status final_status);

   private:
    bool completed_ GUARDED_BY_CONTEXT(sequence_checker_){false};
    const bool need_encryption_key_;
    std::vector<EncryptedRecord> encrypted_records_;
    ScopedReservation encrypted_records_reservation_;
    SEQUENCE_CHECKER(sequence_checker_);

    UploadCallback upload_callback_;
  };

  Uploader(bool need_encryption_key, UploadCallback upload_callback)
      : helper_(base::ThreadPool::CreateSequencedTaskRunner({}),
                need_encryption_key,
                std::move(upload_callback)) {}

  base::SequenceBound<Helper> helper_;
};

ReportingClient::Uploader::Helper::Helper(
    bool need_encryption_key,
    ReportingClient::Uploader::UploadCallback upload_callback)
    : need_encryption_key_(need_encryption_key),
      upload_callback_(std::move(upload_callback)) {
  // Constructor is called on the task assigned by |SequenceBound|.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ReportingClient::Uploader::Helper::ProcessRecord(
    EncryptedRecord data,
    ScopedReservation scoped_reservation,
    base::OnceCallback<void(bool)> processed_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (completed_) {
    std::move(processed_cb).Run(false);
    return;
  }
  encrypted_records_.emplace_back(std::move(data));
  encrypted_records_reservation_.HandOver(scoped_reservation);
  std::move(processed_cb).Run(true);
}

void ReportingClient::Uploader::Helper::ProcessGap(
    SequenceInformation start,
    uint64_t count,
    base::OnceCallback<void(bool)> processed_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (completed_) {
    std::move(processed_cb).Run(false);
    return;
  }
  for (uint64_t i = 0; i < count; ++i) {
    encrypted_records_.emplace_back();
    *encrypted_records_.rbegin()->mutable_sequence_information() = start;
    start.set_sequencing_id(start.sequencing_id() + 1);
  }
  std::move(processed_cb).Run(true);
}

void ReportingClient::Uploader::Helper::Completed(Status final_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!final_status.ok()) {
    // No work to do - something went wrong with storage and it no longer
    // wants to upload the records. Let the records die with |this|.
    return;
  }
  if (completed_) {
    // Upload has already been invoked. Return.
    return;
  }
  completed_ = true;
  if (encrypted_records_.empty() && !need_encryption_key_) {
    return;
  }
  CHECK(upload_callback_);
  Status upload_status =
      std::move(upload_callback_)
          .Run(need_encryption_key_, std::move(encrypted_records_),
               std::move(encrypted_records_reservation_));
  if (!upload_status.ok()) {
    LOG(ERROR) << "Unable to upload records: " << upload_status;
  }
}

// static
std::unique_ptr<EncryptedReportingUploadProvider>
ReportingClient::CreateLocalUploadProvider(
    scoped_refptr<StorageModuleInterface> storage_module) {
  // Note: access local storage inside the callbacks, because it may be not
  // yet stored in the client at the moment EncryptedReportingUploadProvider
  // is instantiated.
  return std::make_unique<EncryptedReportingUploadProvider>(
      base::BindPostTask(
          ReportingClient::GetInstance()->sequenced_task_runner(),
          StorageSelector::GetLocalReportSuccessfulUploadCb(storage_module)),
      base::BindPostTask(
          ReportingClient::GetInstance()->sequenced_task_runner(),
          StorageSelector::GetLocalEncryptionKeyAttachedCb(storage_module)));
}

// static
void ReportingClient::AsyncStartUploader(
    UploaderInterface::UploadReason reason,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
  ReportingClient::GetInstance()->DeliverAsyncStartUploader(
      reason, std::move(start_uploader_cb));
}

void ReportingClient::DeliverAsyncStartUploader(
    UploaderInterface::UploadReason reason,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
  sequenced_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](UploaderInterface::UploadReason reason,
             UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
             ReportingClient* instance) {
            if (!instance->upload_provider_) {
              // If non-missived uploading is enabled, it will need upload
              // provider. In case of missived Uploader will be provided by
              // EncryptedReportingServiceProvider so it does not need to be
              // enabled here.
              if (!StorageSelector::is_uploader_required() ||
                  StorageSelector::is_use_missive()) {
                std::move(start_uploader_cb)
                    .Run(base::unexpected(
                        Status(error::UNAVAILABLE, "Uploader not available")));
                return;
              }
              instance->upload_provider_ =
                  CreateLocalUploadProvider(instance->storage());
            }
            auto uploader = Uploader::Create(
                /*need_encryption_key=*/
                reason == UploaderInterface::UploadReason::KEY_DELIVERY,
                base::BindOnce(
                    [](EncryptedReportingUploadProvider* upload_provider,
                       bool need_encryption_key,
                       std::vector<EncryptedRecord> records,
                       ScopedReservation scoped_reservation) {
                      upload_provider->RequestUploadEncryptedRecords(
                          need_encryption_key, std::move(records),
                          std::move(scoped_reservation), base::DoNothing());
                      return Status::StatusOK();
                    },
                    base::Unretained(instance->upload_provider_.get())));
            std::move(start_uploader_cb).Run(std::move(uploader));
          },
          reason, std::move(start_uploader_cb), base::Unretained(this)));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
}  // namespace reporting
