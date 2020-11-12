// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_context_base.h"

class GURL;

// Manages user permissions for Background Fetch. Background Fetch permission
// is currently dynamic and relies on either the download status from
// DownloadRequestLimiter, or the Automatic Downloads content setting
// This is why it isn't persisted.
class BackgroundFetchPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit BackgroundFetchPermissionContext(
      content::BrowserContext* browser_context);
  ~BackgroundFetchPermissionContext() override = default;

 private:
  // PermissionContextBase implementation.
  bool IsRestrictedToSecureOrigins() const override;
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void DecidePermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;
  void NotifyPermissionSet(const permissions::PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           permissions::BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting) override;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchPermissionContext);
};

#endif  // CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_PERMISSION_CONTEXT_H_
