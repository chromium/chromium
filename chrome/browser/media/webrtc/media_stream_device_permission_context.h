// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_context_base.h"

// Common class which handles the mic and camera permissions.
class MediaStreamDevicePermissionContext
    : public permissions::PermissionContextBase {
 public:
  MediaStreamDevicePermissionContext(content::BrowserContext* browser_context,
                                     ContentSettingsType content_settings_type);

  MediaStreamDevicePermissionContext(
      const MediaStreamDevicePermissionContext&) = delete;
  MediaStreamDevicePermissionContext& operator=(
      const MediaStreamDevicePermissionContext&) = delete;

  ~MediaStreamDevicePermissionContext() override;

  // PermissionContextBase:
  void DecidePermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;

  // TODO(xhwang): GURL.DeprecatedGetOriginAsURL() shouldn't be used as the
  // origin. Need to refactor to use url::Origin. crbug.com/527149 is filed for
  // this.
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  void ResetPermission(const GURL& requesting_origin,
                       const GURL& embedding_origin) override;

 private:
  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;

  ContentSettingsType content_settings_type_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
