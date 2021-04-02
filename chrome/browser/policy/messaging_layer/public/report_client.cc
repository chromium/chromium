// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_impl.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/reporting/client/report_queue_configuration.h"
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
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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
      base::OnceCallback<Status(std::unique_ptr<std::vector<EncryptedRecord>>)>;

  static StatusOr<std::unique_ptr<Uploader>> Create(
      UploadCallback upload_callback) {
    auto uploader = base::WrapUnique(new Uploader(std::move(upload_callback)));
    return uploader;
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
    explicit Helper(UploadCallback upload_callback);
    void ProcessRecord(EncryptedRecord data,
                       base::OnceCallback<void(bool)> processed_cb);
    void ProcessGap(SequencingInformation start,
                    uint64_t count,
                    base::OnceCallback<void(bool)> processed_cb);
    void Completed(Status final_status);

   private:
    bool completed_{false};
    std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records_;

    UploadCallback upload_callback_;
  };

  explicit Uploader(UploadCallback upload_callback)
      : helper_(base::ThreadPool::CreateSequencedTaskRunner({}),
                std::move(upload_callback)) {}

  base::SequenceBound<Helper> helper_;
};

ReportingClient::Uploader::Helper::Helper(
    ReportingClient::Uploader::UploadCallback upload_callback)
    : encrypted_records_(std::make_unique<std::vector<EncryptedRecord>>()),
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
  if (encrypted_records_->empty()) {
    return;
  }
  DCHECK(upload_callback_);
  Status upload_status =
      std::move(upload_callback_).Run(std::move(encrypted_records_));
  if (!upload_status.ok()) {
    LOG(ERROR) << "Unable to upload records: " << upload_status;
  }
}

ReportQueueProvider::InitializingContext*
ReportingClient::InstantiateInitializingContext(
    InitializingContext::UpdateConfigurationCallback update_config_cb,
    InitCompleteCallback init_complete_cb,
    scoped_refptr<InitializationStateTracker> init_state_tracker) {
  return new ClientInitializingContext(
      std::move(build_cloud_policy_client_cb_),
      base::BindRepeating(&ReportingClient::AsyncStartUploader),
      std::move(update_config_cb), std::move(init_complete_cb), this,
      init_state_tracker);
}

ReportingClient::ClientInitializingContext::ClientInitializingContext(
    GetCloudPolicyClientCallback get_client_cb,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    UpdateConfigurationCallback update_config_cb,
    InitCompleteCallback init_complete_cb,
    ReportingClient* client,
    scoped_refptr<ReportingClient::InitializationStateTracker>
        init_state_tracker)
    : ReportQueueProvider::InitializingContext(std::move(update_config_cb),
                                               std::move(init_complete_cb),
                                               std::move(init_state_tracker)),
      get_client_cb_(std::move(get_client_cb)),
      async_start_upload_cb_(std::move(async_start_upload_cb)),
      client_(client) {}

ReportingClient::ClientInitializingContext::~ClientInitializingContext() =
    default;

void ReportingClient::ClientInitializingContext::OnStart() {
  if (!StorageSelector::is_uploader_required()) {
    // Uploading is disabled, proceed with no CloudPolicyClient.
    ConfigureStorageModule();
    return;
  }

  // Uploading is enabled, it will need CloudPolicyClient, which requires
  // posting to the main UI thread for getting access to it.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](GetCloudPolicyClientCallback get_client_cb,
             base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>
                 on_client_configured) {
            std::move(get_client_cb).Run(std::move(on_client_configured));
          },
          std::move(get_client_cb_),
          base::BindOnce(
              &ClientInitializingContext::OnCloudPolicyClientConfigured,
              base::Unretained(this))));
}

void ReportingClient::ClientInitializingContext::OnCloudPolicyClientConfigured(
    StatusOr<policy::CloudPolicyClient*> client_result) {
  if (!client_result.ok()) {
    Complete(Status(error::FAILED_PRECONDITION,
                    base::StrCat({"Unable to build CloudPolicyClient: ",
                                  client_result.status().message()})));
    return;
  }
  cloud_policy_client_ = client_result.ValueOrDie();
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&ClientInitializingContext::ConfigureStorageModule,
                     base::Unretained(this)));
}

void ReportingClient::ClientInitializingContext::ConfigureStorageModule() {
  // Storage location in the local file system (if local storage is enabled).
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    Complete(
        Status(error::FAILED_PRECONDITION, "Could not retrieve base path"));
    return;
  }
  base::FilePath reporting_path = user_data_dir.Append(kReportingDirectory);

  StorageSelector::CreateStorageModule(
      reporting_path, std::move(async_start_upload_cb_),
      base::BindOnce(&ClientInitializingContext::OnStorageModuleConfigured,
                     base::Unretained(this)));
}

void ReportingClient::ClientInitializingContext::OnStorageModuleConfigured(
    StatusOr<scoped_refptr<StorageModuleInterface>> storage_result) {
  if (!storage_result.ok()) {
    Complete(Status(error::FAILED_PRECONDITION,
                    base::StrCat({"Unable to build StorageModule: ",
                                  storage_result.status().message()})));
    return;
  }

  storage_ = storage_result.ValueOrDie();
  if (!cloud_policy_client_) {
    // No policy client - no uploader needed (uploads will not be forwarded).
    Complete(Status::StatusOK());
    return;
  }

  UploadClient::Create(
      cloud_policy_client_,
      base::BindRepeating(&StorageModuleInterface::ReportSuccess, storage_),
      base::BindRepeating(&StorageModuleInterface::UpdateEncryptionKey,
                          storage_),
      base::BindOnce(&ClientInitializingContext::OnUploadClientCreated,
                     base::Unretained(this)));
}

void ReportingClient::ClientInitializingContext::OnUploadClientCreated(
    StatusOr<std::unique_ptr<UploadClient>> upload_client_result) {
  if (!upload_client_result.ok()) {
    Complete(Status(error::FAILED_PRECONDITION,
                    base::StrCat({"Unable to create UploadClient: ",
                                  upload_client_result.status().message()})));
    return;
  }
  upload_client_ = std::move(upload_client_result.ValueOrDie());
  // All done, return success.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(&ClientInitializingContext::Complete,
                                base::Unretained(this), Status::StatusOK()));
}

void ReportingClient::ClientInitializingContext::OnCompleted() {
  DCHECK(!client_->cloud_policy_client_)
      << "Cloud policy client already recorded";
  client_->cloud_policy_client_ = cloud_policy_client_;
  DCHECK(!client_->upload_client_) << "Upload client already recorded";
  client_->upload_client_ = std::move(upload_client_);
  DCHECK(!client_->storage_) << "Storage module already recorded";
  client_->storage_ = std::move(storage_);
}

ReportingClient::ReportingClient()
    : build_cloud_policy_client_cb_(GetCloudPolicyClientCb()) {}

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

// static
void ReportingClient::AsyncStartUploader(
    Priority priority,
    bool need_encryption_key,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
  ReportingClient* const instance =
      static_cast<ReportingClient*>(GetInstance());
  DCHECK(instance->upload_client_);
  auto uploader = Uploader::Create(base::BindOnce(
      &UploadClient::EnqueueUpload,
      base::Unretained(instance->upload_client_.get()), need_encryption_key));
  std::move(start_uploader_cb).Run(std::move(uploader));
}

ReportingClient::TestEnvironment::TestEnvironment(
    policy::CloudPolicyClient* client)
    : saved_build_cloud_policy_client_cb_(
          std::move(static_cast<ReportingClient*>(GetInstance())
                        ->build_cloud_policy_client_cb_)) {
  static_cast<ReportingClient*>(GetInstance())->build_cloud_policy_client_cb_ =
      base::BindOnce(
          [](policy::CloudPolicyClient* client,
             base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>
                 build_cb) { std::move(build_cb).Run(std::move(client)); },
          std::move(client));
}

ReportingClient::TestEnvironment::~TestEnvironment() {
  static_cast<ReportingClient*>(ReportingClient::GetInstance())
      ->build_cloud_policy_client_cb_ =
      std::move(saved_build_cloud_policy_client_cb_);
  base::Singleton<ReportingClient>::OnExit(nullptr);
}

}  // namespace reporting
