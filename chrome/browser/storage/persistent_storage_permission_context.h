// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_PERSISTENT_STORAGE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_STORAGE_PERSISTENT_STORAGE_PERMISSION_CONTEXT_H_

#include <memory>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_request_data.h"

class PersistentStoragePermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  explicit PersistentStoragePermissionContext(
      content::BrowserContext* browser_context);

  PersistentStoragePermissionContext(
      const PersistentStoragePermissionContext&) = delete;
  PersistentStoragePermissionContext& operator=(
      const PersistentStoragePermissionContext&) = delete;

  ~PersistentStoragePermissionContext() override = default;

  // ContentSettingPermissionContextBase:
  void DecidePermission(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback) override;
  void UpdateContentSetting(
      const permissions::PermissionRequestData& request_data,
      ContentSetting content_setting,
      bool is_one_time) override;
};

#endif  // CHROME_BROWSER_STORAGE_PERSISTENT_STORAGE_PERMISSION_CONTEXT_H_
