// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"

namespace ash {

// EncryptedReportingServiceProvider is the link between Missive and
// |reporting::UploadClient|. Missive is a daemon on ChromeOS that encrypts and
// stores |reporting::Records|. |reporting::Records| contain events and messages
// for enterprise customers to monitor their fleet. |reporting::UploadClient|
// uploads these messages to the backend service.
class EncryptedReportingServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  // Resulting |reporting::UploadClient| will handle uploading requests to the
  // server. In order to do this it requires a |policy::CloudPolicyClient|.
  // |policy::CloudPolicyClient| may or may not be ready, so we attempt to get
  // it, and if we fail we repost with a backoff. Until an UploadClient is
  // built, all requests to |RequestUploadEncryptedRecord| will fail.
  using UploadClientBuilderCb = base::RepeatingCallback<void(
      scoped_refptr<reporting::StorageModuleInterface>,
      policy::CloudPolicyClient*,
      reporting::UploadClient::CreatedCallback)>;

  explicit EncryptedReportingServiceProvider(
      reporting::GetCloudPolicyClientCallback build_cloud_policy_client_cb =
          reporting::GetCloudPolicyClientCb(),
      UploadClientBuilderCb upload_client_builder_cb =
          EncryptedReportingServiceProvider::GetUploadClientBuilder());
  EncryptedReportingServiceProvider(
      const EncryptedReportingServiceProvider& other) = delete;
  EncryptedReportingServiceProvider& operator=(
      const EncryptedReportingServiceProvider& other) = delete;
  ~EncryptedReportingServiceProvider() override;

  // Returns true if the current thread is on the origin thread.
  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // EncryptedReportingServiceProvider helper class.
  class UploadHelper;

  // Called when DBus Method is invoked.
  void RequestUploadEncryptedRecord(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Called from ExportedObject when one of the service methods is exported as a
  // DBus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Default provider of upload client builder.
  static UploadClientBuilderCb GetUploadClientBuilder();

  // Returns true if called on the dBus origin thread.
  bool OnOriginThread() const;

  // dBus origin thread and task runner.
  const base::PlatformThreadId origin_thread_id_;
  const scoped_refptr<base::SingleThreadTaskRunner> origin_thread_runner_;

  // UploadHelper object.
  const scoped_refptr<UploadHelper> helper_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<EncryptedReportingServiceProvider> weak_ptr_factory_{
      this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::EncryptedReportingServiceProvider;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_
