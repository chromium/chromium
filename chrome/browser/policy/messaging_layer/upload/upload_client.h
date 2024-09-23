// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/server_uploader.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// UploadClient handles sending records to the correct upload service.
class UploadClient {
 public:
  // UpdateConfigInMissiveCallback is called if the configuration file obtained
  // from the server is different from the one that was sent previously using
  // this callback.
  using UpdateConfigInMissiveCallback =
      ::reporting::UpdateConfigInMissiveCallback;

  // CreatedCallback gets a result of Upload client creation (unique pointer or
  // error status).
  using CreatedCallback =
      base::OnceCallback<void(StatusOr<std::unique_ptr<UploadClient>>)>;

  static void Create(CreatedCallback created_cb);

  virtual ~UploadClient();
  UploadClient(const UploadClient& other) = delete;
  UploadClient& operator=(const UploadClient& other) = delete;

  // Enqueues upload and provides the callbacks to track it:
  // - `enqueued_cb` is called once the upload is enqueued (not started!);
  // - `report_upload_success_cb` and `encryption_key_attached_cb` are called
  // when the upload is responded by the server.
  virtual void EnqueueUpload(
      bool need_encryption_key,
      int config_file_version,
      std::vector<EncryptedRecord> record,
      ScopedReservation scoped_reservation,
      UploadEnqueuedCallback enqueued_cb,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      ConfigFileAttachedCallback config_file_attached_cb);

 protected:
  UploadClient();

 private:
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_
