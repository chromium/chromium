// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "net/base/backoff_entry.h"

namespace chromeos {

// EncryptedReportingServiceProvider is the link between Missive and
// |reporting::UploadClient|. Missive is a daemon on ChromeOS that encrypts and
// stores |reporting::Records|. |reporting::Records| contain events and messages
// for enterprise customers to monitor their fleet. |reporting::UploadClient|
// uploads these messages to the backend service.
class EncryptedReportingServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  explicit EncryptedReportingServiceProvider(
      reporting::GetCloudPolicyClientCallback build_cloud_policy_client_cb =
          reporting::GetCloudPolicyClientCb());
  EncryptedReportingServiceProvider(
      const EncryptedReportingServiceProvider& other) = delete;
  EncryptedReportingServiceProvider& operator=(
      const EncryptedReportingServiceProvider& other) = delete;
  ~EncryptedReportingServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 protected:
  // |reporting::UploadClient| will handle uploading requests to the server. In
  // order to do this it requires a |policy::CloudPolicyClient|.
  // |policy::CloudPolicyClient| may or may not be ready, so we attempt to get
  // it, and if we fail we repost with a backoff. Until an UploadClient is
  // built, all requests to |RequestUploadEncryptedRecord| will fail.
  virtual void BuildUploadClient(
      policy::CloudPolicyClient* client,
      reporting::UploadClient::CreatedCallback update_upload_client_cb);

 private:
  // Called from ExportedObject when one of the service methods is exported as a
  // DBus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void PostNewCloudPolicyClientRequest();
  void OnCloudPolicyClientResult(
      reporting::StatusOr<policy::CloudPolicyClient*> client_result);
  void UpdateUploadClient(std::unique_ptr<reporting::UploadClient> client);
  void OnUploadClientResult(
      reporting::StatusOr<std::unique_ptr<reporting::UploadClient>>
          client_result);
  void RequestUploadEncryptedRecord(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Sequence task runner and checker used during
  // |PostNewCloudPolicyClientRequest| processing.
  // It is also used to protect |upload_client_|.
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequenced_task_checker_);

  reporting::GetCloudPolicyClientCallback build_cloud_policy_client_cb_;
  std::atomic<bool> upload_client_request_in_progress_{false};
  const std::unique_ptr<::net::BackoffEntry> backoff_entry_;
  std::unique_ptr<reporting::UploadClient> upload_client_;
  scoped_refptr<reporting::StorageModuleInterface> storage_module_;

  // Keep these last so that all weak pointers will be invalidated at the
  // beginning of destruction.

  // Weak pointers factory for methods exported by |ExportMethod|.
  base::WeakPtrFactory<EncryptedReportingServiceProvider> weak_ptr_factory_{
      this};

  // Weak pointers factory for methods scheduled on |sequenced_task_runner_|
  // for internal processing of |PostNewCloudPolicyClientRequest|.
  // We cannot reuse |weak_ptr_factory_| - weak pointers must all be
  // dereferenced on a single sequenced task runner, and |weak_ptr_factory| is
  // unrelated to |sequenced_task_runner_|.
  base::WeakPtrFactory<EncryptedReportingServiceProvider>
      internal_weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_
