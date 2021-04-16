// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FAKE_UPLOAD_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FAKE_UPLOAD_CLIENT_H_

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

namespace reporting {

// Instead of a mock, a fake is used so that the callbacks are appropriately
// used based on the response of CloudPolicyClient. CloudPolicyClient should be
// a MockCloudPolicyClient with the responses set as appropriate.
class FakeUploadClient : public UploadClient {
 public:
  ~FakeUploadClient() override;

  static void Create(
      policy::CloudPolicyClient* cloud_policy_client,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      base::OnceCallback<void(StatusOr<std::unique_ptr<UploadClient>>)>
          created_cb);

  Status EnqueueUpload(
      bool need_encryption_keys,
      std::unique_ptr<std::vector<EncryptedRecord>> records) override;

 private:
  FakeUploadClient(policy::CloudPolicyClient* cloud_policy_client,
                   ReportSuccessfulUploadCallback report_upload_success_cb,
                   EncryptionKeyAttachedCallback encryption_key_attached_cb);

  void OnUploadComplete(base::Optional<base::Value> response);

  policy::CloudPolicyClient* const cloud_policy_client_;
  ReportSuccessfulUploadCallback report_upload_success_cb_;
  EncryptionKeyAttachedCallback encryption_key_attached_cb_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FAKE_UPLOAD_CLIENT_H_
