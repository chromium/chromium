// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_

#include <memory>
#include <utility>

#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/shared_queue.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

// ReportingClient acts a single point for creating |reporting::ReportQueue|s.
// It ensures that all ReportQueues are created with the same storage settings.
//
// In order to utilize the ReportingClient the EncryptedReportingPipeline
// feature must be turned on using --enable-features=EncryptedReportingPipeline.
//
// Example Usage:
// void SendMessage(google::protobuf::ImportantMessage important_message,
//                  reporting::ReportQueue::EnqueueCallback done_cb) {
//   // Create configuration.
//   auto config_result = reporting::ReportQueueConfiguration::Create(...);
//   // Bail out if configuration failed to create.
//   if (!config_result.ok()) {
//     std::move(done_cb).Run(config_result.status());
//     return;
//   }
//   // Asynchronously create ReportingQueue.
//   base::ThreadPool::PostTask(
//       FROM_HERE,
//       base::BindOnce(
//           [](google::protobuf::ImportantMessage important_message,
//              reporting::ReportQueue::EnqueueCallback done_cb,
//              std::unique_ptr<reporting::ReportQueueConfiguration> config) {
//             // Asynchronously create ReportingQueue.
//             reporting::ReportingClient::CreateReportQueue(
//                 std::move(config),
//                 base::BindOnce(
//                     [](base::StringPiece data,
//                        reporting::ReportQueue::EnqueueCallback done_cb,
//                        reporting::StatusOr<std::unique_ptr<
//                            reporting::ReportQueue>> report_queue_result) {
//                       // Bail out if queue failed to create.
//                       if (!report_queue_result.ok()) {
//                         std::move(done_cb).Run(report_queue_result.status());
//                         return;
//                       }
//                       // Queue created successfully, enqueue the message.
//                       report_queue_result.ValueOrDie()->Enqueue(
//                           important_message, std::move(done_cb));
//                     },
//                     important_message, std::move(done_cb)));
//           },
//           important_message, std::move(done_cb),
//           std::move(config_result.ValueOrDie())))
// }

class ReportingClient {
 public:
  struct Configuration {
    Configuration();
    ~Configuration();

    // TODO(chromium:1078512) Passing around a raw pointer is unsafe. Wrap
    // CloudPolicyClient and guard access.
    policy::CloudPolicyClient* cloud_policy_client;
    scoped_refptr<StorageModule> storage;
  };

  using CreateReportQueueResponse = StatusOr<std::unique_ptr<ReportQueue>>;

  using CreateReportQueueCallback =
      base::OnceCallback<void(CreateReportQueueResponse)>;

  using UpdateConfigurationCallback =
      base::OnceCallback<void(std::unique_ptr<Configuration>,
                              base::OnceCallback<void(Status)>)>;
  using GetCloudPolicyClientCallback = base::OnceCallback<void(
      base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>)>;

  using InitCompleteCallback = base::OnceCallback<void(Status)>;

  using InitializationStatusCallback = base::OnceCallback<void(Status)>;

  class InitializationStateTracker
      : public base::RefCountedThreadSafe<InitializationStateTracker> {
   public:
    using ReleaseLeaderCallback = base::OnceCallback<void(bool)>;
    using LeaderPromotionRequestCallback =
        base::OnceCallback<void(StatusOr<ReleaseLeaderCallback>)>;
    using GetInitStateCallback = base::OnceCallback<void(bool)>;

    static scoped_refptr<InitializationStateTracker> Create();

    // Will call |get_init_state_cb| with |is_initialized_| value.
    void GetInitState(GetInitStateCallback get_init_state_cb);

    // Will promote one initializer to leader at a time. Will deny
    // initialization requests if the ReportingClient is already initialized. If
    // there are no errors will return a ReleaseLeaderCallback for releasing the
    // initializing leadership.
    //
    // Error code responses:
    // RESOURCE_EXHAUSTED - Returned when a promotion is requested when there is
    //     already a leader.
    // FAILED_PRECONDITION - Returned when a promotion is requested when
    //     ReportingClient is already initialized.
    void RequestLeaderPromotion(
        LeaderPromotionRequestCallback promo_request_cb);

   private:
    friend class base::RefCountedThreadSafe<InitializationStateTracker>;
    InitializationStateTracker();
    virtual ~InitializationStateTracker();

    void OnIsInitializedRequest(GetInitStateCallback get_init_state_cb);

    void OnLeaderPromotionRequest(
        LeaderPromotionRequestCallback promo_request_cb);

    void ReleaseLeader(bool initialization_successful);
    void OnLeaderRelease(bool initialization_successful);

    bool has_promoted_initializing_context_{false};
    bool is_initialized_{false};

    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  };

  class InitializingContext : public TaskRunnerContext<Status> {
   public:
    InitializingContext(
        GetCloudPolicyClientCallback get_client_cb,
        Storage::StartUploadCb start_upload_cb,
        UpdateConfigurationCallback update_config_cb,
        InitCompleteCallback init_complete_cb,
        scoped_refptr<InitializationStateTracker> init_state_tracker,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

   private:
    ~InitializingContext() override;

    // OnStart will begin the process of configuring the ReportClient.
    void OnStart() override;
    void OnLeaderPromotionResult(
        StatusOr<InitializationStateTracker::ReleaseLeaderCallback>
            promo_result);

    void ConfigureCloudPolicyClient();
    void OnCloudPolicyClientConfigured(
        StatusOr<policy::CloudPolicyClient*> client_result);

    // ConfigureStorageModule will build a StorageModule and add it to the
    // |client_config_|.
    void ConfigureStorageModule();
    void OnStorageModuleConfigured(
        StatusOr<scoped_refptr<StorageModule>> storage_result);

    void CreateUploadClient();
    void OnUploadClientCreated(
        StatusOr<std::unique_ptr<UploadClient>> upload_client_result);

    void UpdateConfiguration(std::unique_ptr<UploadClient> upload_client);

    // Complete calls response with |client_config_|
    void Complete(Status status);

    GetCloudPolicyClientCallback get_client_cb_;
    Storage::StartUploadCb start_upload_cb_;
    UpdateConfigurationCallback update_config_cb_;
    scoped_refptr<InitializationStateTracker> init_state_tracker_;

    InitializationStateTracker::ReleaseLeaderCallback release_leader_cb_;
    std::unique_ptr<Configuration> client_config_;
  };

  // RAII class for testing ReportingClient - substitutes a cloud policy client
  // builder to return given client and resets it when destructed.
  class TestEnvironment {
   public:
    explicit TestEnvironment(policy::CloudPolicyClient* client);
    TestEnvironment(const TestEnvironment& other) = delete;
    TestEnvironment& operator=(const TestEnvironment& other) = delete;
    ~TestEnvironment();

   private:
    ReportingClient::GetCloudPolicyClientCallback
        saved_build_cloud_policy_client_cb_;
  };

  ~ReportingClient();
  ReportingClient(const ReportingClient& other) = delete;
  ReportingClient& operator=(const ReportingClient& other) = delete;

  // Allows a user to asynchronously create a |ReportQueue|. Will create an
  // underlying ReportingClient if it doesn't exists. The callback will contain
  // an error if |storage_| cannot be instantiated for any reason.
  //
  // TODO(chromium:1078512): Once the StorageModule is ready, update this
  // comment with concrete failure conditions.
  static void CreateReportQueue(
      std::unique_ptr<ReportQueueConfiguration> config,
      CreateReportQueueCallback create_cb);

  static bool IsEncryptedReportingPipelineEnabled();
  static const base::Feature kEncryptedReportingPipeline;

 private:
  class Uploader;

  // Holds the creation request for a ReportQueue.
  class CreateReportQueueRequest {
   public:
    CreateReportQueueRequest(std::unique_ptr<ReportQueueConfiguration> config,
                             CreateReportQueueCallback create_cb);
    ~CreateReportQueueRequest();
    CreateReportQueueRequest(CreateReportQueueRequest&& other);

    std::unique_ptr<ReportQueueConfiguration> config();
    CreateReportQueueCallback create_cb();

   private:
    std::unique_ptr<ReportQueueConfiguration> config_;
    CreateReportQueueCallback create_cb_;
  };

  friend struct base::DefaultSingletonTraits<ReportingClient>;
  friend class TestEnvironment;

  ReportingClient();

  // Access to singleton instance of ReportingClient.
  static ReportingClient* GetInstance();

  void OnPushComplete();
  void OnInitState(bool reporting_client_configured);
  void OnConfigResult(std::unique_ptr<Configuration> config,
                      base::OnceCallback<void(Status)> continue_init_cb);
  void OnInitializationComplete(Status init_status);

  void ClearRequestQueue(base::queue<CreateReportQueueRequest> failed_requests);
  void BuildRequestQueue(StatusOr<CreateReportQueueRequest> pop_result);

  // TODO(chromium:1078512) Priority is unused, remove it.
  static StatusOr<std::unique_ptr<Storage::UploaderInterface>> BuildUploader(
      Priority priority);

  // Queue for storing creation requests while the ReportingClient is
  // initializing.
  scoped_refptr<SharedQueue<CreateReportQueueRequest>> create_request_queue_;
  scoped_refptr<InitializationStateTracker> init_state_tracker_;
  GetCloudPolicyClientCallback build_cloud_policy_client_cb_;

  scoped_refptr<StorageModule> storage_;
  std::unique_ptr<UploadClient> upload_client_;
  std::unique_ptr<Configuration> config_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
