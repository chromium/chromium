// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_

#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// `RecordHandlerImpl` handles `ReportRequests`, sending them to
// the server, cancelling any in progress reports if a new report is added.
// For that reason `RecordHandlerImpl` ensures that only one report is ever
// processed at one time by forming a queue.
class RecordHandlerImpl : public DmServerUploadService::RecordHandler {
 public:
  RecordHandlerImpl();
  ~RecordHandlerImpl() override;

  // Base class RecordHandler method implementation.
  void HandleRecords(bool need_encryption_key,
                     std::vector<EncryptedRecord> record,
                     ScopedReservation scoped_reservation,
                     DmServerUploadService::CompletionCallback upload_complete,
                     DmServerUploadService::EncryptionKeyAttachedCallback
                         encryption_key_attached_cb) override;

 private:
  // Helper |ReportUploader| class handles enqueuing events on the
  // |report_queue_|.
  class ReportUploader;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_
