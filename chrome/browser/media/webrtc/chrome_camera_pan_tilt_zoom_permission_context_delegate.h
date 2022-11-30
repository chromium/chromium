// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CHROME_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CHROME_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"
#include "extensions/buildflags/buildflags.h"

class ChromeCameraPanTiltZoomPermissionContextDelegate
    : public permissions::CameraPanTiltZoomPermissionContext::Delegate {
 public:
  explicit ChromeCameraPanTiltZoomPermissionContextDelegate(
      content::BrowserContext* browser_context);
  ChromeCameraPanTiltZoomPermissionContextDelegate(
      const ChromeCameraPanTiltZoomPermissionContextDelegate&) = delete;
  ChromeCameraPanTiltZoomPermissionContextDelegate& operator=(
      const ChromeCameraPanTiltZoomPermissionContextDelegate&) = delete;
  ~ChromeCameraPanTiltZoomPermissionContextDelegate() override;

  // CameraPanTiltZoomPermissionContext::Delegate:
  bool GetPermissionStatusInternal(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      ContentSetting* content_setting_result) override;

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Returns true if it's an extension that has the "cameraMove" permission
  // declared in their manifest.
  bool IsPermissionGrantedForExtension(const GURL& origin) const;
#endif

  // Unused on Android so annotated as [[maybe_unused]].
  [[maybe_unused]] raw_ptr<content::BrowserContext> browser_context_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CHROME_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_DELEGATE_H_
