// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_

#include <memory>
#include <vector>

#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// UploadClient handles sending records to the correct upload service.
class UploadClient {
 public:
  // ReportSuccessfulUploadCallback is used to pass server responses back to
  // the owner of |this| (the respone consists of sequencing information and
  // forceConfirm flag).
  using ReportSuccessfulUploadCallback =
      DmServerUploadService::ReportSuccessfulUploadCallback;

  // ReceivedEncryptionKeyCallback is called if server attached encryption key
  // to the response.
  using EncryptionKeyAttachedCallback =
      base::RepeatingCallback<void(SignedEncryptionInfo)>;

  static void Create(
      policy::CloudPolicyClient* cloud_policy_client,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      base::OnceCallback<void(StatusOr<std::unique_ptr<UploadClient>>)>
          created_cb);

  virtual ~UploadClient();
  UploadClient(const UploadClient& other) = delete;
  UploadClient& operator=(const UploadClient& other) = delete;

  virtual Status EnqueueUpload(
      bool need_encryption_keys,
      std::unique_ptr<std::vector<EncryptedRecord>> record);

 protected:
  UploadClient();

 private:
  std::unique_ptr<DmServerUploadService> dm_server_upload_service_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_
