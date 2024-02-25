// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DISPLAY_CAPTURE_DISPLAY_CAPTURE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_DISPLAY_CAPTURE_DISPLAY_CAPTURE_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class DisplayCapturePermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit DisplayCapturePermissionContext(
      content::BrowserContext* browser_context);
  ~DisplayCapturePermissionContext() override = default;

  DisplayCapturePermissionContext(const DisplayCapturePermissionContext&) =
      delete;
  DisplayCapturePermissionContext& operator=(
      const DisplayCapturePermissionContext&) = delete;

 protected:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;

  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting,
                            bool is_one_time) override;
};

#endif  // CHROME_BROWSER_DISPLAY_CAPTURE_DISPLAY_CAPTURE_PERMISSION_CONTEXT_H_
