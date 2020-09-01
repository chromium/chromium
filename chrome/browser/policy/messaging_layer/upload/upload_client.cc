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
StatusOr<std::unique_ptr<UploadClient>> UploadClient::Create(
    std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client,
    ReportSuccessfulUploadCallback report_success_cb) {
  auto upload_client = base::WrapUnique(new UploadClient());
  ASSIGN_OR_RETURN(upload_client->dm_server_upload_service_,
                   DmServerUploadService::Create(std::move(cloud_policy_client),
                                                 report_success_cb));

  return upload_client;
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
