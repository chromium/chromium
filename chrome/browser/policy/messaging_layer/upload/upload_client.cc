// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

namespace reporting {

namespace {

std::unique_ptr<FileUploadJob::Delegate> CreateFileUploadDelegate() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<FileUploadDelegate>();
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
  // No file uploads for all other configurations.
  return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
}  // namespace

// static
void UploadClient::Create(CreatedCallback created_cb) {
  std::move(created_cb).Run(base::WrapUnique(new UploadClient()));
}

Status UploadClient::EnqueueUpload(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb) {
  if (records.empty() && !need_encryption_key) {
    // Do nothing, just return success.
    return Status::StatusOK();
  }

  Start<DmServerUploader>(need_encryption_key, config_file_version,
                          std::move(records), std::move(scoped_reservation),
                          handler_.get(), std::move(report_upload_success_cb),
                          std::move(encryption_key_attached_cb),
                          base::DoNothing(), sequenced_task_runner_);
  // Actual outcome is reported through callbacks; here we just confirm
  // the upload has started.
  return Status::StatusOK();
}

UploadClient::UploadClient()
    : sequenced_task_runner_(content::GetUIThreadTaskRunner()),
      handler_(
          std::make_unique<RecordHandlerImpl>(sequenced_task_runner_,
                                              CreateFileUploadDelegate())) {}

UploadClient::~UploadClient() = default;

}  // namespace reporting
