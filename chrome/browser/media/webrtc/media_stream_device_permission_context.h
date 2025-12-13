// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_decision.h"

// Common class which handles the mic and camera permissions.
class MediaStreamDevicePermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  MediaStreamDevicePermissionContext(content::BrowserContext* browser_context,
                                     ContentSettingsType content_settings_type);

  MediaStreamDevicePermissionContext(
      const MediaStreamDevicePermissionContext&) = delete;
  MediaStreamDevicePermissionContext& operator=(
      const MediaStreamDevicePermissionContext&) = delete;

  ~MediaStreamDevicePermissionContext() override;

  // PermissionContextBase:
#if BUILDFLAG(IS_ANDROID)
  void NotifyPermissionSet(
      const permissions::PermissionRequestData& request_data,
      permissions::BrowserPermissionCallback callback,
      bool persist,
      PermissionDecision decision,
      bool is_final_decision) override;
#endif
  void ResetPermission(const GURL& requesting_origin,
                       const GURL& embedding_origin) override;

  // ContentSettingPermissionContextBase:
  // TODO(xhwang): GURL.DeprecatedGetOriginAsURL() shouldn't be used as the
  // origin. Need to refactor to use url::Origin. crbug.com/527149 is filed for
  // this.
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 private:
#if BUILDFLAG(IS_ANDROID)
  // ContentSettingPermissionContextBase:
  void UpdateTabContext(const permissions::PermissionRequestData& request_data,
                        bool allowed) override;

  void OnAndroidPermissionDecided(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      permissions::BrowserPermissionCallback callback,
      bool permission_granted);
#endif

  ContentSettingsType content_settings_type_;

  // Must be the last member.
  base::WeakPtrFactory<MediaStreamDevicePermissionContext> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
