// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_H_

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/permissions/permission_context_base.h"

class DurableStoragePermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit DurableStoragePermissionContext(
      content::BrowserContext* browser_context);

  DurableStoragePermissionContext(const DurableStoragePermissionContext&) =
      delete;
  DurableStoragePermissionContext& operator=(
      const DurableStoragePermissionContext&) = delete;

  ~DurableStoragePermissionContext() override = default;

  // PermissionContextBase implementation.
  // Grant if requesting_origin is bookmarked.
  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;
  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting,
                            bool is_one_time) override;
};

#endif  // CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_H_
