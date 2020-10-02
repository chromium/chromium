// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_

#include <memory>
#include <utility>

#include "base/containers/queue.h"
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
// Example Usage:
// Status SendMessage(google::protobuf::ImportantMessage important_message,
//                    base::OnceCallback<void(Status)> callback) {
//   ASSIGN_OR_RETURN(std::unique_ptr<ReportQueueConfiguration> config,
//                  ReportQueueConfiguration::Create(...));
//   ASSIGN_OR_RETURN(std::unique_ptr<ReportQueue> report_queue,
//                  ReportingClient::CreateReportQueue(config));
//   return report_queue->Enqueue(important_message, callback);
// }
class ReportingClient {
 public:
  struct Configuration {
    Configuration();
    ~Configuration();

    std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client;
    scoped_refptr<StorageModule> storage;
  };

  using CreateReportQueueResponse = StatusOr<std::unique_ptr<ReportQueue>>;

  using CreateReportQueueCallback =
      base::OnceCallback<void(CreateReportQueueResponse)>;

  using UpdateConfigurationCallback =
      base::OnceCallback<void(std::unique_ptr<Configuration>,
                              base::OnceCallback<void(Status)>)>;
  using BuildCloudPolicyClientCallback = base::OnceCallback<void(
      base::OnceCallback<void(
          StatusOr<std::unique_ptr<policy::CloudPolicyClient>>)>)>;

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
        BuildCloudPolicyClientCallback build_client_cb,
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
        StatusOr<std::unique_ptr<policy::CloudPolicyClient>> client_result);

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

    BuildCloudPolicyClientCallback build_client_cb_;
    Storage::StartUploadCb start_upload_cb_;
    UpdateConfigurationCallback update_config_cb_;
    scoped_refptr<InitializationStateTracker> init_state_tracker_;

    InitializationStateTracker::ReleaseLeaderCallback release_leader_cb_;
    std::unique_ptr<Configuration> client_config_;
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

  // Sets up the ReportingClient for testing with a specified CloudPolicyClient.
  static void Setup_test(std::unique_ptr<policy::CloudPolicyClient> client);
  // Resets the singleton object. Should only be used in tests when the current
  // TaskEnvironment will be invalidated.
  static void Reset_test();

 private:
  // Uploader is passed to Storage in order to upload messages using the
  // UploadClient.
  class Uploader : public Storage::UploaderInterface {
   public:
    using UploadCallback = base::OnceCallback<Status(
        std::unique_ptr<std::vector<EncryptedRecord>>)>;

    static StatusOr<std::unique_ptr<Uploader>> Create(
        UploadCallback upload_callback);

    ~Uploader() override;
    Uploader(const Uploader& other) = delete;
    Uploader& operator=(const Uploader& other) = delete;

    void ProcessRecord(StatusOr<EncryptedRecord> data,
                       base::OnceCallback<void(bool)> processed_cb) override;

    void Completed(Status final_status) override;

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

  ReportingClient();
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
  BuildCloudPolicyClientCallback build_cloud_policy_client_cb_;

  scoped_refptr<StorageModule> storage_;
  std::unique_ptr<UploadClient> upload_client_;
  std::unique_ptr<Configuration> config_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
