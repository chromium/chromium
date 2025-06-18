// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/display_capture/display_capture_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

DisplayCapturePermissionContext::DisplayCapturePermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::DISPLAY_CAPTURE,
          network::mojom::PermissionsPolicyFeature::kDisplayCapture) {}

void DisplayCapturePermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  NotifyPermissionSet(*request_data, std::move(callback),
                      /*persist=*/false, PermissionDecision::kNone,
                      /*is_final_decision=*/true);
}

ContentSetting DisplayCapturePermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_ASK;
}

void DisplayCapturePermissionContext::UpdateContentSetting(
    const permissions::PermissionRequestData& request_data,
    ContentSetting content_setting,
    bool is_one_time) {
  NOTREACHED();
}
