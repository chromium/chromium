// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_UPLOADER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace enterprise_management {
class AppInstallReportRequest;
}

class Profile;

namespace policy {

// Adapter between the system that captures and stores app push-install event
// logs and the policy system which uploads them to the management server. When
// asked to upload event logs, retrieves them from the log store asynchronously
// and forwards them for upload, scheduling retries with exponential backoff in
// case of upload failures.
class AppInstallEventLogUploader : public CloudPolicyClient::Observer {
 public:
  // The delegate that event logs will be retrieved from.
  class Delegate {
   public:
    // Callback invoked by the delegate with the logs to be uploaded in
    // |report|.
    using SerializationCallback = base::OnceCallback<void(
        const enterprise_management::AppInstallReportRequest* report)>;

    // Requests that the delegate serialize the current logs into a protobuf
    // and pass it to |callback|.
    virtual void SerializeForUpload(SerializationCallback callback) = 0;

    // Notification to the delegate that the logs passed via the most recent
    // |SerializationCallback| have been successfully uploaded to the server and
    // can be pruned from storage.
    virtual void OnUploadSuccess() = 0;

   protected:
    virtual ~Delegate();
  };

  // |client| must outlive |this|.
  AppInstallEventLogUploader(CloudPolicyClient* client, Profile* profile);
  ~AppInstallEventLogUploader() override;

  // Sets the delegate. The delegate must either outlive |this| or be explicitly
  // removed by calling |SetDelegate(nullptr)|. Removing the delegate cancels
  // the pending log upload, if any.
  void SetDelegate(Delegate* delegate);

  // Requests log upload. If there is no pending upload yet, asks the delegate
  // to serialize the current logs into a protobuf and sends this to the server.
  // If an upload is pending already, no new upload is scheduled. However, the
  // delegate is notified when the pending upload succeeds and may request
  // another upload at that point.
  //
  // Once requested, the upload is retried with exponential backoff until it
  // succeeds. For each retry, the delegate is asked to serialize logs anew, so
  // that the most recent logs are uploaded.
  //
  // Must only be calling after setting a delegate.
  void RequestUpload();

  // Cancels the pending log upload, if any. Any log currently being serialized
  // by the delegate or uploaded by the client is discarded.
  void CancelUpload();

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override {}
  // Uploads are only possible while the client is registered with the server.
  // If an upload is requested while the client is not registered, the request
  // is stored until the client registers. If the client loses its registration
  // while an upload is pending, the upload is canceled and stored for retry
  // when the client registers again. A stored request is handled as a brand new
  // request when the client registers, by asking the delegate to serialize logs
  // and with the exponential backoff reset to its minimum.
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override {}

 private:
  // Asks the delegate to serialize the current logs into a protobuf and pass it
  // a callback.
  void StartSerialization();

  // Callback invoked by the delegate with the logs to be uploaded in |report|.
  // Forwards the logs to the client for upload.
  void OnSerialized(
      const enterprise_management::AppInstallReportRequest* report);

  // Notification by the client that the most recent log upload has succeeded if
  // |success| is |true| or retries have been exhausted if |success| is |false|.
  // Forwards success to the delegate and schedules a retry with exponential
  // backoff in case of failure.
  void OnUploadDone(bool success);

  // The client used to upload logs to the server.
  CloudPolicyClient* client_ = nullptr;

  // Profile used to fetch the context attributes for report request.
  Profile* profile_ = nullptr;

  // The delegate that provides serialized logs to be uploaded.
  Delegate* delegate_ = nullptr;

  // |true| if log upload has been requested and not completed yet.
  bool upload_requested_ = false;

  // The backoff, in milliseconds, for the next upload retry.
  int retry_backoff_ms_;

  // Weak pointer factory for invalidating callbacks passed to the delegate and
  // scheduled retries when the upload request is canceled or |this| is
  // destroyed.
  base::WeakPtrFactory<AppInstallEventLogUploader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogUploader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_UPLOADER_H_
