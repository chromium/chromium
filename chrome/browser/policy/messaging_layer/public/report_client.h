// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace reporting {

// ReportingClient is an implementation of ReportQueueProvider for Chrome
// (currently only primary Chrome is supported, and so the client expects
// cloud policy client to be available and creates an uploader to use it.

class ReportingClient : public ReportQueueProvider,
                        public ReportingServerConnector::Observer {
 public:
  // RAII class for testing ReportingClient - substitutes reporting files
  // location, signature verification public key and a cloud policy client
  // builder to return given client. Resets client when destructed.
  class TestEnvironment;

  // Factory method creates client to be deletable on the provided task runner.
  // It also registers it as current `ReportQueueProvider`.
  // This registration is reset when the client is deleted.
  static SmartPtr<ReportingClient> Create(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  ~ReportingClient() override;
  ReportingClient(const ReportingClient& other) = delete;
  ReportingClient& operator=(const ReportingClient& other) = delete;

 private:
#if !BUILDFLAG(IS_CHROMEOS)
  class Uploader;
#endif  // !BUILDFLAG(IS_CHROMEOS)
  friend class ReportQueueProvider;
  friend class TestEnvironment;

  // Constructor to be used by factory only.
  explicit ReportingClient(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // Configures the report queue config with an appropriate DM token after its
  // retrieval for downstream processing, and triggers the corresponding
  // completion callback with the updated config.
  void ConfigureReportQueue(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config,
      ReportQueueProvider::ReportQueueConfiguredCallback completion_cb)
      override;

  // ReportingServerConnector::Observer implementation.
  void OnConnected() override;
  void OnDisconnected() override;

#if !BUILDFLAG(IS_CHROMEOS)
  //
  // Everything below is used in Local storage case only.
  //
  static std::unique_ptr<EncryptedReportingUploadProvider>
  CreateLocalUploadProvider(
      scoped_refptr<StorageModuleInterface> storage_module);

  static void AsyncStartUploader(
      base::WeakPtr<ReportQueueProvider> instance,
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  void DeliverAsyncStartUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  // Upload provider (if enabled).
  std::unique_ptr<EncryptedReportingUploadProvider> upload_provider_;
#endif  // !BUILDFLAG(IS_CHROMEOS)
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
