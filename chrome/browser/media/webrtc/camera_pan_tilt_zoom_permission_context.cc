// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/camera_pan_tilt_zoom_permission_context.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"

CameraPanTiltZoomPermissionContext::CameraPanTiltZoomPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {
  host_content_settings_map_ =
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
  host_content_settings_map_->AddObserver(this);
}

CameraPanTiltZoomPermissionContext::~CameraPanTiltZoomPermissionContext() {
  host_content_settings_map_->RemoveObserver(this);
}

void CameraPanTiltZoomPermissionContext::RequestPermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (HasAvailableCameraPtzDevices()) {
    PermissionContextBase::RequestPermission(web_contents, id,
                                             requesting_frame_origin,
                                             user_gesture, std::move(callback));
    return;
  }

  // If there is no camera with PTZ capabilities, let's request a "regular"
  // camera permission instead.
  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      id.render_process_id(), id.render_frame_id());
  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  permission_manager->RequestPermission(ContentSettingsType::MEDIASTREAM_CAMERA,
                                        frame, requesting_frame_origin,
                                        user_gesture, std::move(callback));
}

#if defined(OS_ANDROID)
ContentSetting CameraPanTiltZoomPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // The PTZ permission is automatically granted on Android. It is safe to do so
  // because pan and tilt are not supported on Android.
  return CONTENT_SETTING_ALLOW;
}
#endif

bool CameraPanTiltZoomPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

void CameraPanTiltZoomPermissionContext::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type != ContentSettingsType::MEDIASTREAM_CAMERA &&
      content_type != ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    return;
  }

  // Skip if the camera permission is currently being updated to match camera
  // PTZ permission as OnContentSettingChanged would have been called again
  // causing a reentrancy issue.
  if (updating_mediastream_camera_permission_) {
    updating_mediastream_camera_permission_ = false;
    return;
  }

  // Skip if the camera PTZ permission is currently being reset when camera
  // permission got blocked or reset as OnContentSettingChanged would have been
  // called again causing a reentrancy issue.
  if (updating_camera_ptz_permission_) {
    updating_camera_ptz_permission_ = false;
    return;
  }

  // TODO(crbug.com/1078272): We should not need to deduce the url from the
  // primary pattern here. Modify the infrastructure to facilitate this
  // particular use case better.
  const GURL url(primary_pattern.ToString());
  if (url::Origin::Create(url).opaque())
    return;

  ContentSetting camera_ptz_setting =
      host_content_settings_map_->GetContentSetting(
          url, url, content_settings_type(), resource_identifier);

  if (content_type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    // Automatically update camera permission to camera PTZ permission as any
    // change to camera PTZ should be reflected to camera.
    updating_mediastream_camera_permission_ = true;
    host_content_settings_map_->SetContentSettingCustomScope(
        primary_pattern, secondary_pattern,
        ContentSettingsType::MEDIASTREAM_CAMERA, resource_identifier,
        camera_ptz_setting);
    return;
  }

  // Don't reset camera PTZ permission if it is already blocked or in a
  // "default" state.
  if (camera_ptz_setting == CONTENT_SETTING_BLOCK ||
      camera_ptz_setting == CONTENT_SETTING_ASK) {
    return;
  }

  ContentSetting mediastream_camera_setting =
      host_content_settings_map_->GetContentSetting(url, url, content_type,
                                                    resource_identifier);
  if (mediastream_camera_setting == CONTENT_SETTING_BLOCK ||
      mediastream_camera_setting == CONTENT_SETTING_ASK) {
    // Automatically reset camera PTZ permission if camera permission
    // gets blocked or reset.
    updating_camera_ptz_permission_ = true;
    host_content_settings_map_->SetContentSettingCustomScope(
        primary_pattern, secondary_pattern,
        ContentSettingsType::CAMERA_PAN_TILT_ZOOM, resource_identifier,
        CONTENT_SETTING_DEFAULT);
  }
}

bool CameraPanTiltZoomPermissionContext::HasAvailableCameraPtzDevices() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::vector<blink::MediaStreamDevice> devices =
      MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices();
  for (const blink::MediaStreamDevice& device : devices) {
    if (device.video_control_support.pan || device.video_control_support.tilt ||
        device.video_control_support.zoom) {
      return true;
    }
  }
  return false;
}
