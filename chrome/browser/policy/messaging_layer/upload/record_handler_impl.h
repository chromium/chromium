// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/util/shared_queue.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {

// |RecordHandlerImpl| handles |ReportRequests|, sending them to
// the server using |CloudPolicyClient|. Since |CloudPolicyClient| will cancel
// any in progress reports if a new report is added, |RecordHandlerImpl|
// ensures that only one report is ever processed at one time by forming a
// queue.
class RecordHandlerImpl : public DmServerUploadService::RecordHandler {
 public:
  explicit RecordHandlerImpl(policy::CloudPolicyClient* client);
  ~RecordHandlerImpl() override;

  // Base class RecordHandler method implementation.
  void HandleRecords(bool need_encryption_key,
                     std::unique_ptr<std::vector<EncryptedRecord>> record,
                     DmServerUploadService::CompletionCallback upload_complete,
                     DmServerUploadService::EncryptionKeyAttachedCallback
                         encryption_key_attached_cb) override;

 private:
  // Helper |ReportUploader| class handles enqueuing events on the
  // |report_queue_|, and uploading those events with the |client_|.
  class ReportUploader;

  // Processes last JSON response received from the server in case of success,
  // or nullopt in case of failures on all attempts.
  void ProcessResponse(const base::Value& response);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_
