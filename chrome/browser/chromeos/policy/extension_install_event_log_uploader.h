// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/install_event_log_uploader_base.h"

namespace enterprise_management {
class ExtensionInstallReportRequest;
}

class Profile;

namespace policy {

class CloudPolicyClient;

// Adapter between the system that captures and stores extension install event
// logs and the policy system which uploads them to the management server.
class ExtensionInstallEventLogUploader : public InstallEventLogUploaderBase {
 public:
  // The delegate that event logs will be retrieved from.
  class Delegate {
   public:
    // Callback invoked by the delegate with the extension logs to be uploaded
    // in |report|.
    using ExtensionLogSerializationCallback = base::OnceCallback<void(
        const enterprise_management::ExtensionInstallReportRequest* report)>;

    // Requests that the delegate serialize the current logs into a protobuf
    // and pass it to |callback|.
    virtual void SerializeExtensionLogForUpload(
        ExtensionLogSerializationCallback callback) = 0;

    // Notification to the delegate that the logs passed via the most recent
    // |ExtensionLogSerializationCallback| have been successfully uploaded to
    // the server and can be pruned from storage.
    virtual void OnExtensionLogUploadSuccess() = 0;

   protected:
    virtual ~Delegate();
  };

  // |client| must outlive |this|.
  ExtensionInstallEventLogUploader(CloudPolicyClient* client, Profile* profile);
  ~ExtensionInstallEventLogUploader() override;

  // Sets the delegate. The delegate must either outlive |this| or be explicitly
  // removed by calling |SetDelegate(nullptr)|. Removing or changing the
  // delegate cancels the pending log upload, if any.
  void SetDelegate(Delegate* delegate);

 private:
  // InstallEventLogUploaderBase:
  void CheckDelegateSet() override;
  void PostTaskForStartSerialization() override;
  void CancelClientUpload() override;
  void OnUploadSuccess() override;
  void StartSerialization() override;

  // Callback invoked by the delegate with the extension logs to be uploaded in
  // |report|. Forwards the logs to the client for upload.
  void OnSerialized(
      const enterprise_management::ExtensionInstallReportRequest* report);

  // The delegate that provides serialized logs to be uploaded.
  Delegate* delegate_ = nullptr;

  // Weak pointer factory for invalidating callbacks passed to the delegate and
  // scheduled retries when the upload request is canceled or |this| is
  // destroyed.
  base::WeakPtrFactory<ExtensionInstallEventLogUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_
