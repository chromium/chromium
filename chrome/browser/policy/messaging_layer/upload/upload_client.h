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
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

// UploadClient handles sending records to the correct upload service.
class UploadClient {
 public:
  // ReportSuccessfulUploadCallback is used to pass server responses back to
  // the owner of |this|.
  using ReportSuccessfulUploadCallback =
      base::RepeatingCallback<void(SequencingInformation)>;

  static void Create(
      std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      base::OnceCallback<void(StatusOr<std::unique_ptr<UploadClient>>)>
          created_cb);

  ~UploadClient();
  UploadClient(const UploadClient& other) = delete;
  UploadClient& operator=(const UploadClient& other) = delete;

  Status EnqueueUpload(std::unique_ptr<std::vector<EncryptedRecord>> record);

 private:
  UploadClient();

  std::unique_ptr<DmServerUploadService> dm_server_upload_service_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_UPLOAD_CLIENT_H_
