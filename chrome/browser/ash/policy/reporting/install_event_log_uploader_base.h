// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UPLOADER_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UPLOADER_BASE_H_

#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

class Profile;

namespace policy {

// Adapter between the system that captures and stores app install event
// logs and the policy system which uploads them to the management server. The
// app refers to extension or ARC++ app. When asked to upload event logs,
// retrieves them from the log store asynchronously and forwards them for
// upload, scheduling retries with exponential backoff in case of upload
// failures.
class InstallEventLogUploaderBase : public CloudPolicyClient::Observer {
 public:
  // |client| must outlive |this|.
  InstallEventLogUploaderBase(CloudPolicyClient* client, Profile* profile);

  // Will construct a non-CloudPolicyClient::Observer version of
  // InstallEventLogUploaderBase.
  // TODO(crbug.com/40689377) This exists to support the move to using
  // reporting::ReportQueue, which owns its own CloudPolicyClient. Once
  // ArcInstallEventLogUploader is ready to move to using
  // reporting::ReportQueue, we can likely do a small refactor removing all
  // references to CloudPolicyClient from InstallEventLogUploaderBase and its
  // children.
  explicit InstallEventLogUploaderBase(Profile* profile);

  ~InstallEventLogUploaderBase() override;

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

  virtual void CancelClientUpload() = 0;
  virtual void CheckDelegateSet() = 0;

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

  // Asks the delegate to serialize the current logs into a protobuf and pass it
  // a callback.
  virtual void StartSerialization() = 0;

  // |result| contains the result of the most recent log upload.
  // Forwards success to the delegate and schedules a retry with exponential
  // backoff in case of failure.
  void OnUploadDone(CloudPolicyClient::Result result);

  // Notifies delegate on success of upload.
  virtual void OnUploadSuccess() = 0;

  // Posts a new task to start serialization after |retry_backoff_ms_| ms.
  virtual void PostTaskForStartSerialization() = 0;

  // The client used to upload logs to the server.
  raw_ptr<CloudPolicyClient> client_ = nullptr;

  // Profile used to fetch the context attributes for report request.
  raw_ptr<Profile> profile_ = nullptr;

  // |true| if log upload has been requested and not completed yet.
  bool upload_requested_ = false;

  // The backoff, in milliseconds, for the next upload retry.
  int retry_backoff_ms_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UPLOADER_BASE_H_
