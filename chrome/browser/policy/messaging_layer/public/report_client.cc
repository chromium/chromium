// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/record.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#else
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif

namespace reporting {

namespace {

// policy::CloudPolicyClient is needed by the UploadClient, but is retrieved in
// two different ways for ChromeOS and non-ChromeOS browsers.
// NOT THREAD SAFE - these functions must be called on the main thread.
// TODO(chromium:1078512) Wrap CloudPolicyClient in a new object so that its
// methods and retrieval are accessed on the correct thread.
void GetCloudPolicyClient(
    base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>
        get_client_cb) {
#if defined(OS_CHROMEOS)
  policy::CloudPolicyManager* cloud_policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
#elif defined(OS_ANDROID)
  // Android doesn't have access to a device level CloudPolicyClient, so get the
  // PrimaryUserProfile CloudPolicyClient.
  policy::CloudPolicyManager* cloud_policy_manager =
      ProfileManager::GetPrimaryUserProfile()->GetUserCloudPolicyManager();
#else
  policy::CloudPolicyManager* cloud_policy_manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
#endif
  if (cloud_policy_manager == nullptr) {
    std::move(get_client_cb)
        .Run(
            Status(error::FAILED_PRECONDITION, "This is not a managed device"));
    return;
  }
  auto* cloud_policy_client = cloud_policy_manager->core()->client();
  if (cloud_policy_client == nullptr) {
    std::move(get_client_cb)
        .Run(Status(error::FAILED_PRECONDITION,
                    "Cloud Policy Client is not available"));
    return;
  }
  std::move(get_client_cb).Run(cloud_policy_client);
}

const base::FilePath::CharType kReportingDirectory[] =
    FILE_PATH_LITERAL("reporting");

}  // namespace

// Uploader is passed to Storage in order to upload messages using the
// UploadClient.
class ReportingClient::Uploader : public Storage::UploaderInterface {
 public:
  using UploadCallback =
      base::OnceCallback<Status(std::unique_ptr<std::vector<EncryptedRecord>>)>;

  static StatusOr<std::unique_ptr<Uploader>> Create(
      UploadCallback upload_callback);

  ~Uploader() override;
  Uploader(const Uploader& other) = delete;
  Uploader& operator=(const Uploader& other) = delete;

  void ProcessRecord(EncryptedRecord data,
                     base::OnceCallback<void(bool)> processed_cb) override;
  void ProcessGap(SequencingInformation start,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override;

  void Completed(bool need_encryption_key, Status final_status) override;

 private:
  explicit Uploader(UploadCallback upload_callback_);

  static void RunUpload(
      UploadCallback upload_callback,
      std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records);

  UploadCallback upload_callback_;

  bool completed_{false};
  std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

ReportingClient::Uploader::Uploader(UploadCallback upload_callback)
    : upload_callback_(std::move(upload_callback)),
      encrypted_records_(std::make_unique<std::vector<EncryptedRecord>>()),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

ReportingClient::Uploader::~Uploader() = default;

StatusOr<std::unique_ptr<ReportingClient::Uploader>>
ReportingClient::Uploader::Create(UploadCallback upload_callback) {
  auto uploader = base::WrapUnique(new Uploader(std::move(upload_callback)));
  return uploader;
}

void ReportingClient::Uploader::ProcessRecord(
    EncryptedRecord data,
    base::OnceCallback<void(bool)> processed_cb) {
  if (completed_) {
    std::move(processed_cb).Run(false);
    return;
  }

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<EncryptedRecord>* records, EncryptedRecord record,
             base::OnceCallback<void(bool)> processed_cb) {
            records->emplace_back(std::move(record));
            std::move(processed_cb).Run(true);
          },
          base::Unretained(encrypted_records_.get()), std::move(data),
          std::move(processed_cb)));
}

void ReportingClient::Uploader::ProcessGap(
    SequencingInformation start,
    uint64_t count,
    base::OnceCallback<void(bool)> processed_cb) {
  if (completed_) {
    std::move(processed_cb).Run(false);
    return;
  }

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<EncryptedRecord>* records, SequencingInformation start,
             uint64_t count, base::OnceCallback<void(bool)> processed_cb) {
            EncryptedRecord record;
            *record.mutable_sequencing_information() = std::move(start);
            for (uint64_t i = 0; i < count; ++i) {
              records->emplace_back(record);
              record.mutable_sequencing_information()->set_sequencing_id(
                  record.sequencing_information().sequencing_id() + 1);
            }
            std::move(processed_cb).Run(true);
          },
          base::Unretained(encrypted_records_.get()), std::move(start), count,
          std::move(processed_cb)));
}

void ReportingClient::Uploader::Completed(bool need_encryption_key,
                                          Status final_status) {
  if (!final_status.ok()) {
    // No work to do - something went wrong with storage and it no longer wants
    // to upload the records. Let the records die with |this|.
    return;
  }

  if (completed_) {
    // RunUpload has already been invoked. Return.
    return;
  }
  completed_ = true;

  if (need_encryption_key) {
    // Attach encryption key information.
  }

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Uploader::RunUpload, std::move(upload_callback_),
                     std::move(encrypted_records_)));
}

// static
void ReportingClient::Uploader::RunUpload(
    ReportingClient::Uploader::UploadCallback upload_callback,
    std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records) {
  DCHECK(encrypted_records);
  if (encrypted_records->empty()) {
    return;
  }

  Status upload_status =
      std::move(upload_callback).Run(std::move(encrypted_records));
  if (!upload_status.ok()) {
    LOG(ERROR) << "Unable to upload records: " << upload_status;
  }
}

ReportingClient::Configuration::Configuration() = default;
ReportingClient::Configuration::~Configuration() = default;

ReportingClient::InitializationStateTracker::InitializationStateTracker()
    : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

ReportingClient::InitializationStateTracker::~InitializationStateTracker() =
    default;

// static
scoped_refptr<ReportingClient::InitializationStateTracker>
ReportingClient::InitializationStateTracker::Create() {
  return base::WrapRefCounted(
      new ReportingClient::InitializationStateTracker());
}

void ReportingClient::InitializationStateTracker::GetInitState(
    GetInitStateCallback get_init_state_cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReportingClient::InitializationStateTracker::OnIsInitializedRequest,
          this, std::move(get_init_state_cb)));
}

void ReportingClient::InitializationStateTracker::RequestLeaderPromotion(
    LeaderPromotionRequestCallback promo_request_cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ReportingClient::InitializationStateTracker::
                                    OnLeaderPromotionRequest,
                                this, std::move(promo_request_cb)));
}

void ReportingClient::InitializationStateTracker::OnIsInitializedRequest(
    GetInitStateCallback get_init_state_cb) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](GetInitStateCallback get_init_state_cb, bool is_initialized) {
            std::move(get_init_state_cb).Run(is_initialized);
          },
          std::move(get_init_state_cb), is_initialized_));
}

void ReportingClient::InitializationStateTracker::OnLeaderPromotionRequest(
    LeaderPromotionRequestCallback promo_request_cb) {
  StatusOr<ReleaseLeaderCallback> result;
  if (is_initialized_) {
    result = Status(error::FAILED_PRECONDITION,
                    "ReportClient is already configured");
  } else if (has_promoted_initializing_context_) {
    result = Status(error::RESOURCE_EXHAUSTED,
                    "ReportClient already has a lead initializing context.");
  } else {
    result = base::BindOnce(
        &ReportingClient::InitializationStateTracker::ReleaseLeader, this);
  }

  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(
                     [](LeaderPromotionRequestCallback promo_request_cb,
                        StatusOr<ReleaseLeaderCallback> result) {
                       std::move(promo_request_cb).Run(std::move(result));
                     },
                     std::move(promo_request_cb), std::move(result)));
}

void ReportingClient::InitializationStateTracker::ReleaseLeader(
    bool initialization_successful) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReportingClient::InitializationStateTracker::OnLeaderRelease, this,
          initialization_successful));
}

void ReportingClient::InitializationStateTracker::OnLeaderRelease(
    bool initialization_successful) {
  if (initialization_successful) {
    is_initialized_ = true;
  }
  has_promoted_initializing_context_ = false;
}

ReportingClient::CreateReportQueueRequest::CreateReportQueueRequest(
    std::unique_ptr<ReportQueueConfiguration> config,
    CreateReportQueueCallback create_cb)
    : config_(std::move(config)), create_cb_(std::move(create_cb)) {}

ReportingClient::CreateReportQueueRequest::~CreateReportQueueRequest() =
    default;

ReportingClient::CreateReportQueueRequest::CreateReportQueueRequest(
    ReportingClient::CreateReportQueueRequest&& other)
    : config_(other.config()), create_cb_(other.create_cb()) {}

std::unique_ptr<ReportQueueConfiguration>
ReportingClient::CreateReportQueueRequest::config() {
  return std::move(config_);
}

ReportingClient::CreateReportQueueCallback
ReportingClient::CreateReportQueueRequest::create_cb() {
  return std::move(create_cb_);
}

ReportingClient::InitializingContext::InitializingContext(
    GetCloudPolicyClientCallback get_client_cb,
    Storage::StartUploadCb start_upload_cb,
    UpdateConfigurationCallback update_config_cb,
    InitCompleteCallback init_complete_cb,
    scoped_refptr<ReportingClient::InitializationStateTracker>
        init_state_tracker,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<Status>(std::move(init_complete_cb),
                                sequenced_task_runner),
      get_client_cb_(std::move(get_client_cb)),
      start_upload_cb_(std::move(start_upload_cb)),
      update_config_cb_(std::move(update_config_cb)),
      init_state_tracker_(init_state_tracker),
      client_config_(std::make_unique<Configuration>()) {}

ReportingClient::InitializingContext::~InitializingContext() = default;

void ReportingClient::InitializingContext::OnStart() {
  init_state_tracker_->RequestLeaderPromotion(base::BindOnce(
      &ReportingClient::InitializingContext::OnLeaderPromotionResult,
      base::Unretained(this)));
}

void ReportingClient::InitializingContext::OnLeaderPromotionResult(
    StatusOr<ReportingClient::InitializationStateTracker::ReleaseLeaderCallback>
        promo_result) {
  if (promo_result.status().error_code() == error::FAILED_PRECONDITION) {
    // Between building this InitializingContext and attempting to promote to
    // leader, the ReportingClient was configured. Ok response.
    Complete(Status::StatusOK());
    return;
  }

  if (!promo_result.ok()) {
    Complete(promo_result.status());
    return;
  }

  release_leader_cb_ = std::move(promo_result.ValueOrDie());
  Schedule(&ReportingClient::InitializingContext::ConfigureCloudPolicyClient,
           base::Unretained(this));
}

void ReportingClient::InitializingContext::ConfigureCloudPolicyClient() {
  // CloudPolicyClient requires posting to the main UI thread.
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          [](GetCloudPolicyClientCallback get_client_cb,
             base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>
                 on_client_configured) {
            std::move(get_client_cb).Run(std::move(on_client_configured));
          },
          std::move(get_client_cb_),
          base::BindOnce(&ReportingClient::InitializingContext::
                             OnCloudPolicyClientConfigured,
                         base::Unretained(this))));
}

void ReportingClient::InitializingContext::OnCloudPolicyClientConfigured(
    StatusOr<policy::CloudPolicyClient*> client_result) {
  if (!client_result.ok()) {
    Complete(Status(error::FAILED_PRECONDITION,
                    base::StrCat({"Unable to build CloudPolicyClient: ",
                                  client_result.status().message()})));
    return;
  }
  client_config_->cloud_policy_client = std::move(client_result.ValueOrDie());
  Schedule(&ReportingClient::InitializingContext::ConfigureStorageModule,
           base::Unretained(this));
}

void ReportingClient::InitializingContext::ConfigureStorageModule() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    Complete(
        Status(error::FAILED_PRECONDITION, "Could not retrieve base path"));
    return;
  }

  base::FilePath reporting_path = user_data_dir.Append(kReportingDirectory);
  StorageModule::Create(
      StorageOptions().set_directory(reporting_path),
      std::move(start_upload_cb_), base::MakeRefCounted<EncryptionModule>(),
      base::BindOnce(
          &ReportingClient::InitializingContext::OnStorageModuleConfigured,
          base::Unretained(this)));
}

void ReportingClient::InitializingContext::OnStorageModuleConfigured(
    StatusOr<scoped_refptr<StorageModule>> storage_result) {
  if (!storage_result.ok()) {
    Complete(Status(error::FAILED_PRECONDITION,
                    base::StrCat({"Unable to build StorageModule: ",
                                  storage_result.status().message()})));
    return;
  }

  client_config_->storage = storage_result.ValueOrDie();
  Schedule(
      base::BindOnce(&ReportingClient::InitializingContext::CreateUploadClient,
                     base::Unretained(this)));
}

void ReportingClient::InitializingContext::CreateUploadClient() {
  ReportingClient* const instance = GetInstance();
  DCHECK(!instance->upload_client_);
  UploadClient::Create(
      std::move(client_config_->cloud_policy_client),
      base::BindRepeating(&StorageModule::ReportSuccess,
                          client_config_->storage),
      base::BindRepeating(&StorageModule::UpdateEncryptionKey,
                          client_config_->storage),
      base::BindOnce(&InitializingContext::OnUploadClientCreated,
                     base::Unretained(this)));
}

void ReportingClient::InitializingContext::OnUploadClientCreated(
    StatusOr<std::unique_ptr<UploadClient>> upload_client_result) {
  if (!upload_client_result.ok()) {
    Complete(Status(error::FAILED_PRECONDITION,
                    base::StrCat({"Unable to create UploadClient: ",
                                  upload_client_result.status().message()})));
    return;
  }
  Schedule(&ReportingClient::InitializingContext::UpdateConfiguration,
           base::Unretained(this),
           std::move(upload_client_result.ValueOrDie()));
}

void ReportingClient::InitializingContext::UpdateConfiguration(
    std::unique_ptr<UploadClient> upload_client) {
  ReportingClient* const instance = GetInstance();
  DCHECK(!instance->upload_client_);
  instance->upload_client_ = std::move(upload_client);

  std::move(update_config_cb_)
      .Run(std::move(client_config_),
           base::BindOnce(&ReportingClient::InitializingContext::Complete,
                          base::Unretained(this)));
}

void ReportingClient::InitializingContext::Complete(Status status) {
  std::move(release_leader_cb_).Run(/*initialization_successful=*/status.ok());
  Schedule(&ReportingClient::InitializingContext::Response,
           base::Unretained(this), status);
}

ReportingClient::ReportingClient()
    : create_request_queue_(SharedQueue<CreateReportQueueRequest>::Create()),
      init_state_tracker_(
          ReportingClient::InitializationStateTracker::Create()),
      build_cloud_policy_client_cb_(base::BindOnce(&GetCloudPolicyClient)) {}

ReportingClient::~ReportingClient() = default;

ReportingClient* ReportingClient::GetInstance() {
  return base::Singleton<ReportingClient>::get();
}

void ReportingClient::CreateReportQueue(
    std::unique_ptr<ReportQueueConfiguration> config,
    CreateReportQueueCallback create_cb) {
  if (!IsEncryptedReportingPipelineEnabled()) {
    Status not_enabled = Status(
        error::FAILED_PRECONDITION,
        "The Encrypted Reporting Pipeline is not enabled. Please enable it on "
        "the command line using --enable-features=EncryptedReportingPipeline");
    VLOG(1) << not_enabled;
    std::move(create_cb).Run(not_enabled);
    return;
  }
  auto* instance = GetInstance();
  instance->create_request_queue_->Push(
      CreateReportQueueRequest(std::move(config), std::move(create_cb)),
      base::BindOnce(&ReportingClient::OnPushComplete,
                     base::Unretained(instance)));
}

// static
bool ReportingClient::IsEncryptedReportingPipelineEnabled() {
  return base::FeatureList::IsEnabled(kEncryptedReportingPipeline);
}

// static
const base::Feature ReportingClient::kEncryptedReportingPipeline{
    "EncryptedReportingPipeline", base::FEATURE_DISABLED_BY_DEFAULT};

void ReportingClient::OnPushComplete() {
  init_state_tracker_->GetInitState(
      base::BindOnce(&ReportingClient::OnInitState, base::Unretained(this)));
}

void ReportingClient::OnInitState(bool reporting_client_configured) {
  if (!reporting_client_configured) {
    // Schedule an InitializingContext to take care of initialization.
    Start<ReportingClient::InitializingContext>(
        std::move(build_cloud_policy_client_cb_),
        base::BindRepeating(&ReportingClient::BuildUploader),
        base::BindOnce(&ReportingClient::OnConfigResult,
                       base::Unretained(this)),
        base::BindOnce(&ReportingClient::OnInitializationComplete,
                       base::Unretained(this)),
        init_state_tracker_, base::ThreadPool::CreateSequencedTaskRunner({}));
    return;
  }

  // Client was configured, build the queue!
  create_request_queue_->Pop(base::BindOnce(&ReportingClient::BuildRequestQueue,
                                            base::Unretained(this)));
}

void ReportingClient::OnConfigResult(
    std::unique_ptr<ReportingClient::Configuration> config,
    base::OnceCallback<void(Status)> continue_init_cb) {
  config_ = std::move(config);
  std::move(continue_init_cb).Run(Status::StatusOK());
}

void ReportingClient::OnInitializationComplete(Status init_status) {
  if (init_status.error_code() == error::RESOURCE_EXHAUSTED) {
    // This happens when a new request comes in while the ReportingClient is
    // undergoing initialization. The leader will either clear or build the
    // queue when it completes.
    return;
  }

  // Configuration failed. Clear out all the requests that came in while we were
  // attempting to configure.
  if (!init_status.ok()) {
    create_request_queue_->Swap(
        base::queue<CreateReportQueueRequest>(),
        base::BindOnce(&ReportingClient::ClearRequestQueue,
                       base::Unretained(this)));
    return;
  }
  create_request_queue_->Pop(base::BindOnce(&ReportingClient::BuildRequestQueue,
                                            base::Unretained(this)));
}

void ReportingClient::ClearRequestQueue(
    base::queue<CreateReportQueueRequest> failed_requests) {
  while (!failed_requests.empty()) {
    // Post to general thread.
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(
                       [](CreateReportQueueRequest queue_request) {
                         std::move(queue_request.create_cb())
                             .Run(Status(error::UNAVAILABLE,
                                         "Unable to build a ReportQueue"));
                       },
                       std::move(failed_requests.front())));
    failed_requests.pop();
  }
}

void ReportingClient::BuildRequestQueue(
    StatusOr<CreateReportQueueRequest> pop_result) {
  // Queue is clear - nothing more to do.
  if (!pop_result.ok()) {
    return;
  }

  // We don't want to block either the ReportingClient sequenced_task_runner_ or
  // the create_request_queue_.sequenced_task_runner_, so we post the task to a
  // general thread.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<StorageModule> storage_module,
                        CreateReportQueueRequest report_queue_request) {
                       std::move(report_queue_request.create_cb())
                           .Run(ReportQueue::Create(
                               report_queue_request.config(), storage_module));
                     },
                     config_->storage, std::move(pop_result.ValueOrDie())));

  // Build the next item asynchronously
  create_request_queue_->Pop(base::BindOnce(&ReportingClient::BuildRequestQueue,
                                            base::Unretained(this)));
}

// static
StatusOr<std::unique_ptr<Storage::UploaderInterface>>
ReportingClient::BuildUploader(Priority priority) {
  ReportingClient* const instance = GetInstance();
  DCHECK(instance->upload_client_);
  return Uploader::Create(
      base::BindOnce(&UploadClient::EnqueueUpload,
                     base::Unretained(instance->upload_client_.get()),
                     !instance->config_->storage->has_encryption_key()));
}

ReportingClient::TestEnvironment::TestEnvironment(
    policy::CloudPolicyClient* client)
    : saved_build_cloud_policy_client_cb_(std::move(
          ReportingClient::GetInstance()->build_cloud_policy_client_cb_)) {
  ReportingClient::GetInstance()->build_cloud_policy_client_cb_ =
      base::BindOnce(
          [](policy::CloudPolicyClient* client,
             base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>
                 build_cb) { std::move(build_cb).Run(std::move(client)); },
          std::move(client));
}

ReportingClient::TestEnvironment::~TestEnvironment() {
  ReportingClient::GetInstance()->build_cloud_policy_client_cb_ =
      std::move(saved_build_cloud_policy_client_cb_);
  base::Singleton<ReportingClient>::OnExit(nullptr);
}

}  // namespace reporting
