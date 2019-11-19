// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "components/bookmarks/browser/bookmark_model.h"

class DurableStoragePermissionContext : public PermissionContextBase {
 public:
  explicit DurableStoragePermissionContext(Profile* profile);
  ~DurableStoragePermissionContext() override = default;

  // PermissionContextBase implementation.
  // Grant if requesting_origin is bookmarked.
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        BrowserPermissionCallback callback) override;
  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting) override;
  bool IsRestrictedToSecureOrigins() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DurableStoragePermissionContext);
};

#endif  // CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_H_
