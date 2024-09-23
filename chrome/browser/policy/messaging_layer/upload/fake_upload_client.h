// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FAKE_UPLOAD_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FAKE_UPLOAD_CLIENT_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"

namespace reporting {

// Instead of a mock, a fake is used so that the callbacks are appropriately
// used based on the response of CloudPolicyClient. CloudPolicyClient should be
// a MockCloudPolicyClient with the responses set as appropriate.
class FakeUploadClient : public UploadClient {
 public:
  ~FakeUploadClient() override;

  static void Create(CreatedCallback created_cb);

  void EnqueueUpload(
      bool need_encryption_key,
      int config_file_version,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      UploadEnqueuedCallback enqueued_cb,
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      ConfigFileAttachedCallback config_file_attached_cb) override;

 private:
  FakeUploadClient();

  void OnUploadComplete(
      ReportSuccessfulUploadCallback report_upload_success_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      ConfigFileAttachedCallback config_file_attached_cb,
      StatusOr<UploadResponseParser> response);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FAKE_UPLOAD_CLIENT_H_
