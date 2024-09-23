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
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/storage_selector/storage_selector.h"
#include "chrome/browser/policy/messaging_layer/util/dm_token_retriever_provider.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/common/chrome_paths.h"
#include "components/reporting/client/dm_token_retriever.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/reporting_errors.h"
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

ReportingClient::ReportingClient(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
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
                    base::BindPostTask(
                        ReportQueueProvider::GetInstance()
                            ->sequenced_task_runner(),
                        base::BindRepeating(
                            &ReportingClient::AsyncStartUploader,
                            ReportQueueProvider::GetInstance()->GetWeakPtr())),
                    std::move(storage_created_cb));
#endif  // !BUILDFLAG(IS_CHROMEOS)
              }),
          sequenced_task_runner) {
  // Register itself as observer to connector.
  ReportingServerConnector::GetInstance()->AddObserver(this);
}

ReportingClient::~ReportingClient() {
  // Unregister itself as observer to connector.
  ReportingServerConnector::GetInstance()->RemoveObserver(this);
}

void ReportingClient::OnConnected() {
  // Immediately perform dummy upload that will retrieve encryption key, if
  // `Storage` does not have it yet. This is done to provide the key for later
  // events posting even in case the device goes offline very soon after
  // enrollment - `Flush` tries to request encryption key from the server. It
  // is expected to succeed, but even if it fails, later enqueue operations
  // have a good chance to succeed if the server is available. Note that we
  // cannot use Speculative Report Queue - as opposed to `Enqueue`, `Flush`
  // does wait for the underlying actual queue to be created.
  ReportQueueProvider::CreateQueue(
      ReportQueueConfiguration::Create(
          {
              .destination = Destination::HEARTBEAT_EVENTS  // Unused
          })
          .Build()
          .value(),
      base::BindOnce([](StatusOr<std::unique_ptr<ReportQueue>> queue_result) {
        if (!queue_result.has_value()) {
          LOG(WARNING) << "Failed to create queue for initial flush";
          return;
        }
        // Flush SECURITY queue since it is usually empty (events are uploaded
        // immediately after they are posted).
        queue_result.value()->Flush(
            Priority::SECURITY, base::BindOnce([](Status status) {
              LOG_IF(WARNING, !status.ok()) << "Initial flush error=" << status;
            }));
      }));
}

void ReportingClient::OnDisconnected() {
  // No action required upon disconnect.
}

// static
ReportQueueProvider::SmartPtr<ReportingClient> ReportingClient::Create(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  return SmartPtr<ReportingClient>(
      new ReportingClient(sequenced_task_runner),
      base::OnTaskRunnerDeleter(sequenced_task_runner));
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
          ReportQueueProvider::GetInstance()->sequenced_task_runner(),
          StorageSelector::GetLocalReportSuccessfulUploadCb(storage_module)),
      base::BindPostTask(
          ReportQueueProvider::GetInstance()->sequenced_task_runner(),
          StorageSelector::GetLocalEncryptionKeyAttachedCb(storage_module)),
      // The configuration file feature is only available in CrOS.
      /*update_config_in_missive_cb=*/base::DoNothing());
}

// static
void ReportingClient::AsyncStartUploader(
    base::WeakPtr<ReportQueueProvider> instance,
    UploaderInterface::UploadReason reason,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
  if (!instance) {
    std::move(start_uploader_cb)
        .Run(base::unexpected(
            Status(error::UNAVAILABLE, "Client not available")));
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::REPORTING_CLIENT_IS_NULL,
        UnavailableErrorReason::MAX_VALUE);
    return;
  }
  auto* const client = static_cast<ReportingClient*>(instance.get());
  CHECK(client);
  client->DeliverAsyncStartUploader(reason, std::move(start_uploader_cb));
}

void ReportingClient::DeliverAsyncStartUploader(
    UploaderInterface::UploadReason reason,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
  if (!upload_provider_) {
    // If non-missived uploading is enabled, it will need upload
    // provider. In case of missived Uploader will be provided by
    // EncryptedReportingServiceProvider so it does not need to be
    // enabled here.
    if (!StorageSelector::is_uploader_required() ||
        StorageSelector::is_use_missive()) {
      std::move(start_uploader_cb)
          .Run(base::unexpected(
              Status(error::UNAVAILABLE, "Uploader not available")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::UPLOAD_PROVIDER_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    upload_provider_ = CreateLocalUploadProvider(storage());
  }
  auto uploader = Uploader::Create(
      /*need_encryption_key=*/
      reason == UploaderInterface::UploadReason::KEY_DELIVERY,
      base::BindOnce(
          [](base::WeakPtr<EncryptedReportingUploadProvider> upload_provider,
             bool need_encryption_key, std::vector<EncryptedRecord> records,
             ScopedReservation scoped_reservation) {
            if (!upload_provider) {
              base::UmaHistogramEnumeration(
                  reporting::kUmaUnavailableErrorReason,
                  UnavailableErrorReason::UPLOAD_PROVIDER_IS_NULL,
                  UnavailableErrorReason::MAX_VALUE);
              return Status{error::UNAVAILABLE, "Uploader not available"};
            }
            upload_provider->RequestUploadEncryptedRecords(
                need_encryption_key, std::move(records),
                std::move(scoped_reservation), base::DoNothing());
            return Status::StatusOK();
          },
          upload_provider_->GetWeakPtr()));
  std::move(start_uploader_cb).Run(std::move(uploader));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
}  // namespace reporting
