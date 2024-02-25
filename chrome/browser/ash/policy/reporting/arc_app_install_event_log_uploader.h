// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_UPLOADER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_uploader_base.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace enterprise_management {
class AppInstallReportRequest;
}

class Profile;

namespace policy {

// Adapter between the system that captures and stores ARC++ app push-install
// event logs and the policy system which uploads them to the management server.
class ArcAppInstallEventLogUploader : public InstallEventLogUploaderBase {
 public:
  // The delegate that event logs will be retrieved from.
  class Delegate {
   public:
    // Callback invoked by the delegate with the ARC++ logs to be uploaded in
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
  ArcAppInstallEventLogUploader(CloudPolicyClient* client, Profile* profile);
  ~ArcAppInstallEventLogUploader() override;

  // Sets the delegate. The delegate must either outlive |this| or be explicitly
  // removed by calling |SetDelegate(nullptr)|. Removing the delegate cancels
  // the pending log upload, if any.
  void SetDelegate(Delegate* delegate);

 private:
  // InstallEventLogUploaderBase:
  void CheckDelegateSet() override;
  void PostTaskForStartSerialization() override;
  void CancelClientUpload() override;
  void OnUploadSuccess() override;
  void StartSerialization() override;

  // Callback invoked by the delegate with the app logs to be uploaded in
  // |report|. Forwards the logs to the client for upload.
  void OnSerialized(
      const enterprise_management::AppInstallReportRequest* report);

  // The delegate that provides serialized logs to be uploaded.
  raw_ptr<Delegate> delegate_ = nullptr;

  // Weak pointer factory for invalidating callbacks passed to the delegate and
  // scheduled retries when the upload request is canceled or |this| is
  // destroyed.
  base::WeakPtrFactory<ArcAppInstallEventLogUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_UPLOADER_H_
