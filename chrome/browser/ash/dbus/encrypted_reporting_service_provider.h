// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/policy/messaging_layer/storage_selector/storage_selector.h"
#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"

namespace ash {

// EncryptedReportingServiceProvider is the link between Missive and
// `::reporting::UploadClient`. Missive is a daemon on ChromeOS that encrypts
// and stores `::reporting::Records`. `::reporting::Records` contain events and
// messages for enterprise customers to monitor their fleet.
// `::reporting::UploadClient` belongs to Ash Chrome and uploads these messages
// to the backend service.
class EncryptedReportingServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  explicit EncryptedReportingServiceProvider(
      std::unique_ptr<::reporting::EncryptedReportingUploadProvider>
          upload_provider = GetDefaultUploadProvider());
  EncryptedReportingServiceProvider(
      const EncryptedReportingServiceProvider& other) = delete;
  EncryptedReportingServiceProvider& operator=(
      const EncryptedReportingServiceProvider& other) = delete;
  ~EncryptedReportingServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called when DBus Method is invoked.
  void RequestUploadEncryptedRecords(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Called from ExportedObject when one of the service methods is exported as a
  // DBus method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Callbacks referring to MissivedClient.
  static ::reporting::ReportSuccessfulUploadCallback
  GetReportSuccessUploadCallback();
  static ::reporting::EncryptionKeyAttachedCallback
  GetEncryptionKeyAttachedCallback();
  static ::reporting::UpdateConfigInMissiveCallback
  GetUpdateConfigInMissiveCallback();

  // Returns true if called on the origin thread.
  bool OnOriginThread() const;

  // Constructs default upload provider.
  static std::unique_ptr<::reporting::EncryptedReportingUploadProvider>
  GetDefaultUploadProvider();

  // Origin thread and task runner.
  const base::PlatformThreadId origin_thread_id_;
  const scoped_refptr<base::SingleThreadTaskRunner> origin_thread_runner_;

  // Memory resource for upload requests and responses.
  scoped_refptr<::reporting::ResourceManager> memory_resource_;

  // Upload Provider.
  const std::unique_ptr<::reporting::EncryptedReportingUploadProvider>
      upload_provider_;

  // Network condition service.
  ::reporting::NetworkConditionService network_condition_service_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<EncryptedReportingServiceProvider> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_ENCRYPTED_REPORTING_SERVICE_PROVIDER_H_
