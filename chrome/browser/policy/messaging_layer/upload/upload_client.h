// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_uploader.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// UploadClient handles sending records to the correct upload service.
class UploadClient {
 public:
  // ReportSuccessfulUploadCallback is used to pass server responses back to
  // the owner of |this| (the response consists of sequencing information and
  // forceConfirm flag).
  using ReportSuccessfulUploadCallback =
      ::reporting::ReportSuccessfulUploadCallback;

  // ReceivedEncryptionKeyCallback is called if server attached encryption key
  // to the response.
  using EncryptionKeyAttachedCallback =
      ::reporting::EncryptionKeyAttachedCallback;

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

  virtual Status EnqueueUpload(
      bool need_encryption_key,
      std::vector<EncryptedRecord> record,
      ScopedReservation scoped_reservation,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb);

 protected:
  UploadClient();

 private:
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  const std::unique_ptr<RecordHandler> handler_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_
