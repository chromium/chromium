// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace reporting {

// static
void UploadClient::Create(
    std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    base::OnceCallback<void(StatusOr<std::unique_ptr<UploadClient>>)>
        created_cb) {
  auto upload_client = base::WrapUnique(new UploadClient());
  DmServerUploadService::Create(
      std::move(cloud_policy_client), report_upload_success_cb,
      base::BindOnce(
          [](std::unique_ptr<UploadClient> upload_client,
             base::OnceCallback<void(StatusOr<std::unique_ptr<UploadClient>>)>
                 created_cb,
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
    std::unique_ptr<std::vector<EncryptedRecord>> records) {
  DCHECK(records);

  if (records->empty()) {
    return Status::StatusOK();
  }

  return dm_server_upload_service_->EnqueueUpload(std::move(records));
}

UploadClient::UploadClient() = default;

UploadClient::~UploadClient() = default;

}  // namespace reporting
