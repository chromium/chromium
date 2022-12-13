// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
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
  auto upload_client = base::WrapUnique(new UploadClient());
  DmServerUploadService::Create(base::BindOnce(
      [](std::unique_ptr<UploadClient> upload_client,
         CreatedCallback created_cb,
         StatusOr<std::unique_ptr<DmServerUploadService>> uploader) {
        if (!uploader.ok()) {
          std::move(created_cb).Run(uploader.status());
          return;
        }
        upload_client->dm_server_upload_service_ =
            std::move(uploader.ValueOrDie());
        std::move(created_cb).Run(std::move(upload_client));
      },
      std::move(upload_client), std::move(created_cb)));
}

Status UploadClient::EnqueueUpload(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb) {
  if (records.empty() && !need_encryption_key) {
    return Status::StatusOK();
  }

  return dm_server_upload_service_->EnqueueUpload(
      need_encryption_key, std::move(records), std::move(scoped_reservation),
      std::move(report_upload_success_cb),
      std::move(encryption_key_attached_cb));
}

UploadClient::UploadClient() = default;

UploadClient::~UploadClient() = default;

}  // namespace reporting
