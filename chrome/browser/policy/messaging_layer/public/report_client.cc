// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_impl.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_module.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/storage_selector/storage_selector.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace reporting {

namespace {

const base::FilePath::CharType kReportingDirectory[] =
    FILE_PATH_LITERAL("reporting");

}  // namespace

// Uploader is passed to Storage in order to upload messages using the
// UploadClient.
class ReportingClient::Uploader : public UploaderInterface {
 public:
  using UploadCallback =
      base::OnceCallback<Status(bool,
                                std::unique_ptr<std::vector<EncryptedRecord>>)>;

  static std::unique_ptr<Uploader> Create(bool need_encryption_key,
                                          UploadCallback upload_callback) {
    return base::WrapUnique(
        new Uploader(need_encryption_key, std::move(upload_callback)));
  }

  ~Uploader() override = default;
  Uploader(const Uploader& other) = delete;
  Uploader& operator=(const Uploader& other) = delete;

  void ProcessRecord(EncryptedRecord data,
                     base::OnceCallback<void(bool)> processed_cb) override {
    helper_.AsyncCall(&Helper::ProcessRecord)
        .WithArgs(std::move(data), std::move(processed_cb));
  }
  void ProcessGap(SequencingInformation start,
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
                       base::OnceCallback<void(bool)> processed_cb);
    void ProcessGap(SequencingInformation start,
                    uint64_t count,
                    base::OnceCallback<void(bool)> processed_cb);
    void Completed(Status final_status);

   private:
    bool completed_{false};
    const bool need_encryption_key_;
    std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records_;

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
      encrypted_records_(std::make_unique<std::vector<EncryptedRecord>>()),
      upload_callback_(std::move(upload_callback)) {}

void ReportingClient::Uploader::Helper::ProcessRecord(
    EncryptedRecord data,
    base::OnceCallback<void(bool)> processed_cb) {
  if (completed_) {
    std::move(processed_cb).Run(false);
    return;
  }
  encrypted_records_->emplace_back(std::move(data));
  std::move(processed_cb).Run(true);
}

void ReportingClient::Uploader::Helper::ProcessGap(
    SequencingInformation start,
    uint64_t count,
    base::OnceCallback<void(bool)> processed_cb) {
  if (completed_) {
    std::move(processed_cb).Run(false);
    return;
  }
  for (uint64_t i = 0; i < count; ++i) {
    encrypted_records_->emplace_back();
    *encrypted_records_->rbegin()->mutable_sequencing_information() = start;
    start.set_sequencing_id(start.sequencing_id() + 1);
  }
  std::move(processed_cb).Run(true);
}

void ReportingClient::Uploader::Helper::Completed(Status final_status) {
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
  DCHECK(encrypted_records_);
  if (encrypted_records_->empty() && !need_encryption_key_) {
    return;
  }
  DCHECK(upload_callback_);
  Status upload_status =
      std::move(upload_callback_)
          .Run(need_encryption_key_, std::move(encrypted_records_));
  if (!upload_status.ok()) {
    LOG(ERROR) << "Unable to upload records: " << upload_status;
  }
}

class ReportingClient::ClientInitializingContext
    : public ReportQueueProvider::InitializingContext {
 public:
  ClientInitializingContext(
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      InitCompleteCallback init_complete_cb,
      ReportingClient* client,
      scoped_refptr<InitializationStateTracker> init_state_tracker)
      : ReportQueueProvider::InitializingContext(std::move(init_complete_cb),
                                                 std::move(init_state_tracker)),
        async_start_upload_cb_(std::move(async_start_upload_cb)),
        client_(client) {}

 private:
  // Destructor only called from Complete().
  // The class runs a series of callbacks each of which may invoke
  // either the next callback or Complete(). Thus eventually Complete()
  // is always called and InitializingContext instance is self-destruct.
  ~ClientInitializingContext() override = default;

  // Begins the process of configuring the ReportingClient.
  void OnStart() override {
    StorageSelector::CreateStorageModule(
        client_->reporting_path_, client_->verification_key_,
        CompressionInformation::COMPRESSION_SNAPPY,
        std::move(async_start_upload_cb_),
        base::BindPostTask(
            client_->client_sequenced_task_runner_,
            base::BindOnce(
                &ClientInitializingContext::OnStorageModuleConfigured,
                base::Unretained(this))));
  }

  // Handles StorageModuleInterface instantiation for ReportingClient to refer
  // to.
  void OnStorageModuleConfigured(
      StatusOr<scoped_refptr<StorageModuleInterface>> storage_result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(client_->client_sequence_checker_);
    if (!storage_result.ok()) {
      Complete(storage_result.status());
      return;
    }
    DCHECK(!client_->storage_) << "Storage module already recorded";
    client_->storage_ = storage_result.ValueOrDie();
    Complete(Status::StatusOK());
  }

  // Finally updates client with the elements of the configuration into the
  // ReportingClient, if the configuration process succeeded.
  void OnCompleted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(client_->client_sequence_checker_);
  }

  UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;
  ReportingClient* const client_;
};

ReportQueueProvider::InitializingContext*
ReportingClient::InstantiateInitializingContext(
    InitCompleteCallback init_complete_cb,
    scoped_refptr<InitializationStateTracker> init_state_tracker) {
  return new ClientInitializingContext(
      base::BindRepeating(&ReportingClient::AsyncStartUploader),
      std::move(init_complete_cb), this, init_state_tracker);
}

ReportingClient::ReportingClient()
    : verification_key_(SignatureVerifier::VerificationKey()),
      build_cloud_policy_client_cb_(GetCloudPolicyClientCb()),
      client_sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  // Storage location in the local file system (if local storage is enabled).
  base::FilePath user_data_dir;
  const auto res =
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(res) << "Could not retrieve base path";
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_data_dir = user_data_dir.Append("user");
#endif
  reporting_path_ = user_data_dir.Append(kReportingDirectory);
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
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

StatusOr<std::unique_ptr<ReportQueue>> ReportingClient::CreateNewQueue(
    std::unique_ptr<ReportQueueConfiguration> config) {
  return ReportQueueImpl::Create(std::move(config), storage_);
}

StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
ReportingClient::CreateNewSpeculativeQueue() {
  return SpeculativeReportQueueImpl::Create();
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
  client_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](UploaderInterface::UploadReason reason,
             UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
             ReportingClient* instance) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(instance->client_sequence_checker_);
            if (!instance->upload_provider_) {
              // If non-missived uploading is enabled, it will need upload
              // provider, In case of missived Uploader will be provided by
              // EncryptedReportingServiceProvider so it does not need to be
              // enabled here.
              if (StorageSelector::is_uploader_required() &&
                  !StorageSelector::is_use_missive()) {
                DCHECK(!instance->upload_provider_)
                    << "Upload provider already recorded";
                instance->upload_provider_ = instance->GetDefaultUploadProvider(
                    instance->build_cloud_policy_client_cb_);
              } else {
                std::move(start_uploader_cb)
                    .Run(Status(error::UNAVAILABLE, "Uploader not available"));
                return;
              }
            }
            auto uploader = Uploader::Create(
                reason,
                base::BindOnce(
                    [](EncryptedReportingUploadProvider* upload_provider,
                       bool need_encryption_key,
                       std::unique_ptr<std::vector<EncryptedRecord>> records) {
                      upload_provider->RequestUploadEncryptedRecords(
                          need_encryption_key, std::move(records),
                          base::DoNothing());
                      return Status::StatusOK();
                    },
                    base::Unretained(instance->upload_provider_.get())));
            std::move(start_uploader_cb).Run(std::move(uploader));
          },
          reason, std::move(start_uploader_cb), base::Unretained(this)));
}

std::unique_ptr<EncryptedReportingUploadProvider>
ReportingClient::GetDefaultUploadProvider(
    GetCloudPolicyClientCallback build_cloud_policy_client_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  auto report_success_cb = base::BindRepeating(
      [](SequencingInformation sequencing_information, bool force_confirm) {
        ReportingClient* const client = ReportingClient::GetInstance();
        DCHECK_CALLED_ON_VALID_SEQUENCE(client->client_sequence_checker_);
        if (client->storage_) {
          client->storage_->ReportSuccess(std::move(sequencing_information),
                                          force_confirm);
        }
      });
  auto update_encryption_key_cb =
      base::BindRepeating([](SignedEncryptionInfo signed_encryption_info) {
        ReportingClient* const client = ReportingClient::GetInstance();
        DCHECK_CALLED_ON_VALID_SEQUENCE(client->client_sequence_checker_);
        if (client->storage_) {
          client->storage_->UpdateEncryptionKey(
              std::move(signed_encryption_info));
        }
      });
  return std::make_unique<::reporting::EncryptedReportingUploadProvider>(
      base::BindPostTask(client_sequenced_task_runner_, report_success_cb),
      base::BindPostTask(client_sequenced_task_runner_,
                         update_encryption_key_cb),
      build_cloud_policy_client_cb);
}

ReportingClient::TestEnvironment::TestEnvironment(
    const base::FilePath& reporting_path,
    base::StringPiece verification_key,
    policy::CloudPolicyClient* client)
    : saved_build_cloud_policy_client_cb_(std::move(
          ReportingClient::GetInstance()->build_cloud_policy_client_cb_)) {
  ReportingClient::GetInstance()->reporting_path_ = reporting_path;
  ReportingClient::GetInstance()->verification_key_.assign(
      verification_key.data(), verification_key.size());
  ReportingClient::GetInstance()->build_cloud_policy_client_cb_ =
      base::BindRepeating(
          [](policy::CloudPolicyClient* client,
             CloudPolicyClientResultCb build_cb) {
            std::move(build_cb).Run(std::move(client));
          },
          std::move(client));
}

ReportingClient::TestEnvironment::~TestEnvironment() {
  ReportingClient::GetInstance()->build_cloud_policy_client_cb_ =
      std::move(saved_build_cloud_policy_client_cb_);
  base::Singleton<ReportingClient>::OnExit(nullptr);
}

}  // namespace reporting
