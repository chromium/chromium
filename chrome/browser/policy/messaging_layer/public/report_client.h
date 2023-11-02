// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/storage/storage_module.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// ReportingClient is an implementation of ReportQueueProvider for Chrome
// (currently only primary Chrome is supported, and so the client expects
// cloud policy client to be available and creates an uploader to use it.

class ReportingClient : public ReportQueueProvider {
 public:
  // RAII class for testing ReportingClient - substitutes reporting files
  // location, signature verification public key and a cloud policy client
  // builder to return given client. Resets client when destructed.
  class TestEnvironment;

  ~ReportingClient() override;
  ReportingClient(const ReportingClient& other) = delete;
  ReportingClient& operator=(const ReportingClient& other) = delete;

 private:
  class Uploader;

  friend class ReportQueueProvider;
  friend struct base::DefaultSingletonTraits<ReportingClient>;

  // Constructor to be used by singleton only.
  ReportingClient();

  // Accesses singleton ReportingClient instance.
  // Separate from ReportQueueProvider::GetInstance, because
  // Singleton<ReportingClient>::get() can only be used inside ReportingClient
  // class.
  static ReportingClient* GetInstance();

  // Configures the report queue config with an appropriate DM token after its
  // retrieval for downstream processing, and triggers the corresponding
  // completion callback with the updated config.
  void ConfigureReportQueue(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config,
      ReportQueueProvider::ReportQueueConfiguredCallback completion_cb)
      override;

  //
  // Everything below is used in Local storage case only.
  //

  static void CreateLocalStorageModule(
      const base::FilePath& local_reporting_path,
      base::StringPiece verification_key,
      CompressionInformation::CompressionAlgorithm compression_algorithm,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
          cb);

  static StorageModule* GetLocalStorageModule();

  static void AsyncStartUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  void DeliverAsyncStartUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  // Returns upload provider for the client in case of local storage.
  std::unique_ptr<EncryptedReportingUploadProvider> CreateLocalUploadProvider();

  // Upload provider (if enabled).
  std::unique_ptr<EncryptedReportingUploadProvider> upload_provider_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
