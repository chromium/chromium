// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_WEBVIEW_AUTH_DELEGATE_H_
#define ASH_WEBUI_BOCA_UI_WEBVIEW_AUTH_DELEGATE_H_

namespace content {
class BrowserContext;
class StoragePartition;
class StoragePartitionConfig;
}  // namespace content

namespace signin {
class IdentityManager;
}

namespace ash::boca {

class WebviewAuthDelegate {
 public:
  WebviewAuthDelegate();
  WebviewAuthDelegate(const WebviewAuthDelegate&) = delete;
  WebviewAuthDelegate& operator=(const WebviewAuthDelegate&) = delete;
  virtual ~WebviewAuthDelegate();

  // Returns the IdentityManager for the active user profile.
  virtual signin::IdentityManager* GetIdentityManager();

  //  Returns storage partition for a given `context` and
  //  `storage_partition_config`.
  virtual content::StoragePartition* GetStoragePartition(
      content::BrowserContext* context,
      const content::StoragePartitionConfig& storage_partition_config);
};

}  // namespace ash::boca
#endif  // ASH_WEBUI_BOCA_UI_WEBVIEW_AUTH_DELEGATE_H_
