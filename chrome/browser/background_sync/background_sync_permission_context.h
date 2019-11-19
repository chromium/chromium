// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "chrome/browser/permissions/permission_context_base.h"

class Profile;

// Manages user permissions for background sync. The context is scoped to the
// requesting origin, which should always be equal to the top-level origin as
// background syncs can only be requested from top-level pages.
// The permission status is ALLOW by default and can be changed globally or on a
// per-site basis from the content settings page. The user is not prompted for
// permission.
// TODO(nsatragno): actually implement the UI to allow changing the setting.
class BackgroundSyncPermissionContext : public PermissionContextBase {
 public:
  explicit BackgroundSyncPermissionContext(Profile* profile);
  ~BackgroundSyncPermissionContext() override = default;

 private:
  // PermissionContextBase:
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        BrowserPermissionCallback callback) override;
  bool IsRestrictedToSecureOrigins() const override;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncPermissionContext);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
