// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/webview_auth_delegate.h"

#include "base/check.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"

namespace ash::boca {

WebviewAuthDelegate::WebviewAuthDelegate() {}

WebviewAuthDelegate::~WebviewAuthDelegate() = default;

signin::IdentityManager* WebviewAuthDelegate::GetIdentityManager() {
  return BocaAppClient::Get()->GetIdentityManager();
}

content::StoragePartition* WebviewAuthDelegate::GetStoragePartition(
    content::BrowserContext* context,
    const content::StoragePartitionConfig& storage_partition_config) {
  CHECK(context) << "Boca requested identity manager before user session "
                    "start.";
  return context->GetStoragePartition(storage_partition_config);
}

}  // namespace ash::boca
