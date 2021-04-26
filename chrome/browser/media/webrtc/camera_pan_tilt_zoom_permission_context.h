// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_context_base.h"

// Manage user permissions that only control camera movement (pan, tilt, and
// zoom). Those permissions are automatically reset when the "regular" camera
// permission is blocked or reset.
class CameraPanTiltZoomPermissionContext
    : public permissions::PermissionContextBase,
      public content_settings::Observer {
 public:
  explicit CameraPanTiltZoomPermissionContext(
      content::BrowserContext* browser_context);
  ~CameraPanTiltZoomPermissionContext() override;

  CameraPanTiltZoomPermissionContext(
      const CameraPanTiltZoomPermissionContext&) = delete;
  CameraPanTiltZoomPermissionContext& operator=(
      const CameraPanTiltZoomPermissionContext&) = delete;

 private:
  // PermissionContextBase
  void RequestPermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_frame_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;
#if defined(OS_ANDROID)
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
#endif
  bool IsRestrictedToSecureOrigins() const override;

  // content_settings::Observer
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // Returns true if at least one video capture device has PTZ capabilities.
  // Otherwise returns false.
  bool HasAvailableCameraPtzDevices() const;

  HostContentSettingsMap* host_content_settings_map_;

  bool updating_camera_ptz_permission_ = false;
  bool updating_mediastream_camera_permission_ = false;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_
