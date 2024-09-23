// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

BASE_DECLARE_FEATURE(kEnableReportingFromUnmanagedDevices);

// Singleton wrapper of a reporting server client used when uploading events
// to the reporting server. Enables safe access to the cloud policy client with
// an ability to detect when it is disconnected. Actual upload is implemented
// with a dedicated reporting client.
class ReportingServerConnector : public ::policy::CloudPolicyCore::Observer {
 public:
  using ResponseCallback = EncryptedReportingClient::ResponseCallback;

  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnConnected() = 0;
    virtual void OnDisconnected() = 0;

   protected:
    Observer() = default;
  };

  // RAII class for testing ReportingServerConnector - substitutes cloud policy
  // client instead of getting it from the cloud policy core. Resets client when
  // destructed.
  class TestEnvironment;

  ReportingServerConnector(const ReportingServerConnector& other) = delete;
  ReportingServerConnector& operator=(const ReportingServerConnector& other) =
      delete;
  ~ReportingServerConnector() override;

  // Accesses singleton `ReportingServerConnector` instance.
  static ReportingServerConnector* GetInstance();

  // Uploads a report containing `merging_payload` (merged into the default
  // payload of the job). The client must be in a registered state (otherwise
  // the upload fails). The `callback` will be called when the operation
  // completes or fails.
  static void UploadEncryptedReport(bool need_encryption_key,
                                    int config_file_version,
                                    std::vector<EncryptedRecord> records,
                                    ScopedReservation scoped_reservation,
                                    UploadEnqueuedCallback enqueued_cb,
                                    ResponseCallback callback);

  // Adds/removes observer to the Connector.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend struct base::DefaultSingletonTraits<ReportingServerConnector>;

  // Constructor to be used by singleton only.
  ReportingServerConnector();

  // Returns cloud policy manager or Status in case of error.
  StatusOr<::policy::CloudPolicyManager*> GetUserCloudPolicyManager();

  // Returns OK if `CloudPolicyCore` instance is usable, or error status
  // otherwise. If successful, caches the core and registers `this` as its
  // observer.
  Status EnsureUsableCore();

  // Returns OK if `CloudPolicyClient` is already usable or made usable, or
  // error status otherwise. If successful, caches the client.
  Status EnsureUsableClient();

  // ::policy::CloudPolicyCore::Observer implementation.
  void OnCoreConnected(::policy::CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(::policy::CloudPolicyCore* core) override;
  void OnCoreDisconnecting(::policy::CloudPolicyCore* core) override;
  void OnCoreDestruction(::policy::CloudPolicyCore* core) override;

  // Presets uploads and forwards the data.
  void UploadEncryptedReportInternal(bool need_encryption_key,
                                     int config_file_version,
                                     std::vector<EncryptedRecord> records,
                                     ScopedReservation scoped_reservation,
                                     UploadEnqueuedCallback enqueued_cb,
                                     ResponseCallback callback);

  // Onwed by CloudPolicyManager. Cached here (only on UI task runner).
  raw_ptr<::policy::CloudPolicyCore> core_ = nullptr;

  // Onwed by CloudPolicyCore. Used by `UploadEncryptedReport` - must be
  // non-null by then. Cached here (only on UI task runner).
  raw_ptr<::policy::CloudPolicyClient> client_ = nullptr;

  std::unique_ptr<EncryptedReportingClient> encrypted_reporting_client_;

  // Active observers list (to be updated on UI task runner only).
  std::vector<raw_ptr<Observer>> observers_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_
