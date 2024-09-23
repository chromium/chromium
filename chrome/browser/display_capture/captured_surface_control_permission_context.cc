// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/display_capture/captured_surface_control_permission_context.h"

#include "base/feature_list.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

CapturedSurfaceControlPermissionContext::
    CapturedSurfaceControlPermissionContext(
        content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::CAPTURED_SURFACE_CONTROL,
          blink::mojom::PermissionsPolicyFeature::kCapturedSurfaceControl),
      sticky_permissions_(base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlStickyPermissions)) {}

bool CapturedSurfaceControlPermissionContext::UsesAutomaticEmbargo() const {
  return sticky_permissions_;
}

void CapturedSurfaceControlPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  if (sticky_permissions_) {
    permissions::PermissionContextBase::UpdateContentSetting(
        requesting_origin, embedding_origin, content_setting, is_one_time);
  } else {
    // Avoid recording the setting; it is not really associated with
    // the origin, but rather with the individual capture-session.
    // (A capture-session can only be associated with a single origin,
    // but an origin might have multiple capture-sessions.)
  }
}

}  // namespace permissions
