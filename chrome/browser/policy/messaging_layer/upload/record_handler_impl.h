// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_uploader.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// `RecordHandlerImpl` handles `ReportRequests`, sending them to
// the server, cancelling any in progress reports if a new report is added.
// For that reason `RecordHandlerImpl` ensures that only one report is ever
// processed at one time by forming a queue.
class RecordHandlerImpl : public RecordHandler {
 public:
  RecordHandlerImpl(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
      std::unique_ptr<FileUploadJob::Delegate> delegate);
  ~RecordHandlerImpl() override;

  // Base class RecordHandler method implementation.
  void HandleRecords(
      bool need_encryption_key,
      std::vector<EncryptedRecord> record,
      ScopedReservation scoped_reservation,
      CompletionCallback upload_complete,
      EncryptionKeyAttachedCallback encryption_key_attached_cb) override;

  // Helper method adds a record to the default storage on ReportClient task
  // runner and calls `done_cb` with the result..
  static void AddRecordToStorage(Priority priority,
                                 Record record_copy,
                                 base::OnceCallback<void(Status)> done_cb);

 private:
  // Helper `ReportUploader` class handles events being uploaded.
  class ReportUploader;

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  // The next two fields are only used for LOG_UPLOAD events.
  const std::unique_ptr<FileUploadJob::Delegate> delegate_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_RECORD_HANDLER_IMPL_H_
