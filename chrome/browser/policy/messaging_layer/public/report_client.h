// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/storage_selector/storage_selector.h"
#include "components/reporting/util/shared_queue.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// ReportingClient is an implementation of ReportQueueProvider for Chrome
// (currently only primary Chrome is supported, and so the client expects
// cloud policy client to be available and creates an uploader to use it.

class ReportingClient : public ReportQueueProvider {
 public:
  using CreateReportQueueResponse = StatusOr<std::unique_ptr<ReportQueue>>;

  using CreateReportQueueCallback =
      base::OnceCallback<void(CreateReportQueueResponse)>;

  ReportQueueProvider::InitializingContext* InstantiateInitializingContext(
      InitCompleteCallback init_complete_cb,
      scoped_refptr<InitializationStateTracker> init_state_tracker) override;

  StatusOr<std::unique_ptr<ReportQueue>> CreateNewQueue(
      std::unique_ptr<ReportQueueConfiguration> config) override;

  StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
  CreateNewSpeculativeQueue() override;

  // RAII class for testing ReportingClient - substitutes reporting files
  // location, signature verification public key and a cloud policy client
  // builder to return given client. Resets client when destructed.
  class TestEnvironment {
   public:
    TestEnvironment(const base::FilePath& reporting_path,
                    base::StringPiece verification_key,
                    policy::CloudPolicyClient* client);
    TestEnvironment(const TestEnvironment& other) = delete;
    TestEnvironment& operator=(const TestEnvironment& other) = delete;
    ~TestEnvironment();

   private:
    GetCloudPolicyClientCallback saved_build_cloud_policy_client_cb_;
    std::unique_ptr<EncryptedReportingUploadProvider> saved_upload_provider_;
  };

  ~ReportingClient() override;
  ReportingClient(const ReportingClient& other) = delete;
  ReportingClient& operator=(const ReportingClient& other) = delete;

 private:
  class ClientInitializingContext;
  class Uploader;

  friend class TestEnvironment;
  friend class ReportQueueProvider;
  friend struct base::DefaultSingletonTraits<ReportingClient>;

  // Constructor to be used by singleton only.
  ReportingClient();

  // Accesses singleton ReportingClient instance.
  // Separate from ReportQueueProvider::GetInstance, because
  // Singleton<ReportingClient>::get() can only be used inside ReportingClient
  // class.
  static ReportingClient* GetInstance();

  void OnInitState(bool reporting_client_configured);
  void OnInitializationComplete(Status init_status);

  static void AsyncStartUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  void DeliverAsyncStartUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  // Returns default upload provider for the client.
  std::unique_ptr<EncryptedReportingUploadProvider> GetDefaultUploadProvider(
      GetCloudPolicyClientCallback build_cloud_policy_client_cb);

  // Local storage parameters.
  base::FilePath reporting_path_;
  std::string verification_key_;

  // Storage module associated with the client. Protected by
  // client_sequenced_task_runner_.
  scoped_refptr<StorageModuleInterface> storage_;

  // Cloud policy client (set by constructor, may only be changed for tests).
  GetCloudPolicyClientCallback build_cloud_policy_client_cb_;

  // Upload provider (if enabled).
  std::unique_ptr<EncryptedReportingUploadProvider> upload_provider_;

  // Task runner used for guarding the client state.
  scoped_refptr<base::SequencedTaskRunner> client_sequenced_task_runner_;
  SEQUENCE_CHECKER(client_sequence_checker_);
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
