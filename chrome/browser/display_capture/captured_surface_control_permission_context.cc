// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/display_capture/captured_surface_control_permission_context.h"

#include "base/feature_list.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

CapturedSurfaceControlPermissionContext::
    CapturedSurfaceControlPermissionContext(
        content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::CAPTURED_SURFACE_CONTROL,
          network::mojom::PermissionsPolicyFeature::kCapturedSurfaceControl) {}

void CapturedSurfaceControlPermissionContext::UpdateContentSetting(
    const PermissionRequestData& request_data,
    ContentSetting content_setting,
    bool is_one_time) {
  permissions::PermissionContextBase::UpdateContentSetting(
      request_data, content_setting, is_one_time);
}

}  // namespace permissions
