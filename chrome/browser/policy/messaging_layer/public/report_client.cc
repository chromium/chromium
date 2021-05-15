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
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_impl.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
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

ReportingClient::AsyncStartUploaderRequest::AsyncStartUploaderRequest(
    Priority priority,
    bool need_encryption_key,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb)
    : priority_(priority),
      need_encryption_key_(need_encryption_key),
      start_uploader_cb_(std::move(start_uploader_cb)) {}
ReportingClient::AsyncStartUploaderRequest::~AsyncStartUploaderRequest() =
    default;

Priority ReportingClient::AsyncStartUploaderRequest::priority() const {
  return priority_;
}
bool ReportingClient::AsyncStartUploaderRequest::need_encryption_key() const {
  return need_encryption_key_;
}
UploaderInterface::UploaderInterfaceResultCb&
ReportingClient::AsyncStartUploaderRequest::start_uploader_cb() {
  return start_uploader_cb_;
}

// Uploader is passed to Storage in order to upload messages using the
// UploadClient.
class ReportingClient::Uploader : public UploaderInterface {
 public:
  using UploadCallback =
      base::OnceCallback<Status(bool,
                                std::unique_ptr<std::vector<EncryptedRecord>>)>;

  static StatusOr<std::unique_ptr<Uploader>> Create(
      bool need_encryption_key,
      UploadCallback upload_callback) {
    auto uploader = base::WrapUnique(
        new Uploader(need_encryption_key, std::move(upload_callback)));
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

ReportQueueProvider::InitializingContext*
ReportingClient::InstantiateInitializingContext(
    InitCompleteCallback init_complete_cb,
    scoped_refptr<InitializationStateTracker> init_state_tracker) {
  return new ClientInitializingContext(
      build_cloud_policy_client_cb_,
      base::BindRepeating(&ReportingClient::AsyncStartUploader),
      std::move(init_complete_cb), this, init_state_tracker);
}

ReportingClient::ClientInitializingContext::ClientInitializingContext(
    GetCloudPolicyClientCallback get_client_cb,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    InitCompleteCallback init_complete_cb,
    ReportingClient* client,
    scoped_refptr<ReportingClient::InitializationStateTracker>
        init_state_tracker)
    : ReportQueueProvider::InitializingContext(std::move(init_complete_cb),
                                               std::move(init_state_tracker)),
      get_client_cb_(std::move(get_client_cb)),
      async_start_upload_cb_(std::move(async_start_upload_cb)),
      client_(client) {
  DCHECK(get_client_cb_);
}

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
    client_->uploaders_queue_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClientInitializingContext::Complete, base::Unretained(this),
            Status(error::FAILED_PRECONDITION,
                   base::StrCat({"Unable to build CloudPolicyClient: ",
                                 client_result.status().message()}))));
    return;
  }
  cloud_policy_client_ = client_result.ValueOrDie();
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&ClientInitializingContext::ConfigureStorageModule,
                     base::Unretained(this)));
}

void ReportingClient::ClientInitializingContext::ConfigureStorageModule() {
  StorageSelector::CreateStorageModule(
      client_->reporting_path_, client_->verification_key_,
      std::move(async_start_upload_cb_),
      base::BindOnce(&ClientInitializingContext::OnStorageModuleConfigured,
                     base::Unretained(this)));
}

void ReportingClient::ClientInitializingContext::OnStorageModuleConfigured(
    StatusOr<scoped_refptr<StorageModuleInterface>> storage_result) {
  if (!storage_result.ok()) {
    client_->uploaders_queue_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClientInitializingContext::Complete, base::Unretained(this),
            Status(error::FAILED_PRECONDITION,
                   base::StrCat({"Unable to build StorageModule: ",
                                 storage_result.status().message()}))));
    return;
  }

  storage_ = storage_result.ValueOrDie();
  if (!cloud_policy_client_) {
    // No policy client - no uploader needed (uploads will not be forwarded).
    client_->uploaders_queue_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ClientInitializingContext::Complete,
                                  base::Unretained(this), Status::StatusOK()));
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
    client_->uploaders_queue_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClientInitializingContext::Complete, base::Unretained(this),
            Status(error::FAILED_PRECONDITION,
                   base::StrCat({"Unable to create UploadClient: ",
                                 upload_client_result.status().message()}))));
    return;
  }
  upload_client_ = std::move(upload_client_result.ValueOrDie());
  // All done, return success.
  client_->uploaders_queue_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ClientInitializingContext::Complete,
                                base::Unretained(this), Status::StatusOK()));
}

void ReportingClient::ClientInitializingContext::OnCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_->uploaders_queue_sequence_checker_);
  if (cloud_policy_client_) {
    DCHECK(client_->cloud_policy_client_ == nullptr)
        << "Cloud policy client already recorded";
    client_->cloud_policy_client_ = cloud_policy_client_;
  }
  if (upload_client_) {
    DCHECK(!client_->upload_client_) << "Upload client already recorded";
    client_->SetUploadClient(std::move(upload_client_));
  }
  DCHECK(!client_->storage_) << "Storage module already recorded";
  client_->storage_ = std::move(storage_);
}

ReportingClient::ReportingClient()
    : verification_key_(SignatureVerifier::VerificationKey()),
      build_cloud_policy_client_cb_(GetCloudPolicyClientCb()),
      uploaders_queue_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
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
  DETACH_FROM_SEQUENCE(uploaders_queue_sequence_checker_);
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

// static
void ReportingClient::AsyncStartUploader(
    Priority priority,
    bool need_encryption_key,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
  ReportingClient* const instance =
      static_cast<ReportingClient*>(GetInstance());
  instance->DeliverAsyncStartUploader(priority, need_encryption_key,
                                      std::move(start_uploader_cb));
}

void ReportingClient::DeliverAsyncStartUploader(
    Priority priority,
    bool need_encryption_key,
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
  uploaders_queue_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Priority priority, bool need_encryption_key,
             UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
             ReportingClient* instance) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(
                instance->uploaders_queue_sequence_checker_);
            if (instance->upload_client_) {
              auto uploader = Uploader::Create(
                  need_encryption_key,
                  base::BindOnce(
                      &UploadClient::EnqueueUpload,
                      base::Unretained(instance->upload_client_.get())));
              std::move(start_uploader_cb).Run(std::move(uploader));
              return;
            }
            // Not set yet. Enqueue it.
            instance->async_start_uploaders_queue_.emplace(
                priority, need_encryption_key, std::move(start_uploader_cb));
          },
          priority, need_encryption_key, std::move(start_uploader_cb),
          base::Unretained(this)));
}

void ReportingClient::FlushAsyncStartUploaderQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(uploaders_queue_sequence_checker_);
  // Executed on sequential task runner.
  while (!async_start_uploaders_queue_.empty()) {
    auto& request = async_start_uploaders_queue_.front();
    auto uploader = Uploader::Create(
        request.need_encryption_key(),
        base::BindOnce(&UploadClient::EnqueueUpload,
                       base::Unretained(upload_client_.get())));
    std::move(request.start_uploader_cb()).Run(std::move(uploader));
    async_start_uploaders_queue_.pop();
  }
}

void ReportingClient::SetUploadClient(
    std::unique_ptr<UploadClient> upload_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(uploaders_queue_sequence_checker_);
  // This can only happen once.
  DCHECK(!upload_client_);
  upload_client_ = std::move(upload_client);
  uploaders_queue_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ReportingClient::FlushAsyncStartUploaderQueue,
                                base::Unretained(this)));
}

ReportingClient::TestEnvironment::TestEnvironment(
    const base::FilePath& reporting_path,
    base::StringPiece verification_key,
    policy::CloudPolicyClient* client)
    : saved_build_cloud_policy_client_cb_(
          std::move(static_cast<ReportingClient*>(GetInstance())
                        ->build_cloud_policy_client_cb_)) {
  static_cast<ReportingClient*>(GetInstance())->reporting_path_ =
      reporting_path;
  static_cast<ReportingClient*>(GetInstance())->verification_key_ =
      std::string(verification_key);
  static_cast<ReportingClient*>(GetInstance())->build_cloud_policy_client_cb_ =
      base::BindRepeating(
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
