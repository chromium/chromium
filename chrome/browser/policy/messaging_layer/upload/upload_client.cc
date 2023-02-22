// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_uploader.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_impl.h"
#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace reporting {

// static
void UploadClient::Create(CreatedCallback created_cb) {
  std::move(created_cb).Run(base::WrapUnique(new UploadClient()));
}

Status UploadClient::EnqueueUpload(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb) {
  if (records.empty() && !need_encryption_key) {
    // Do nothing, just return success.
    return Status::StatusOK();
  }

  Start<DmServerUploader>(need_encryption_key, std::move(records),
                          std::move(scoped_reservation), handler_.get(),
                          std::move(report_upload_success_cb),
                          std::move(encryption_key_attached_cb),
                          base::DoNothing(), sequenced_task_runner_);
  // Actual outcome is reported through callbacks; here we just confirm
  // the upload has started.
  return Status::StatusOK();
}

UploadClient::UploadClient()
    : sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      handler_(std::make_unique<RecordHandlerImpl>(
          sequenced_task_runner_,
          std::make_unique<FileUploadDelegate>(),
          ReportQueueProvider::GetInstance()->storage())) {}

UploadClient::~UploadClient() = default;

}  // namespace reporting
