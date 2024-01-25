// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/display_capture/captured_surface_control_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

CapturedSurfaceControlPermissionContext::
    CapturedSurfaceControlPermissionContext(
        content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::CAPTURED_SURFACE_CONTROL,
          blink::mojom::PermissionsPolicyFeature::kCapturedSurfaceControl) {}

bool CapturedSurfaceControlPermissionContext::UsesAutomaticEmbargo() const {
  return false;
}

void CapturedSurfaceControlPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  // Avoid recording the setting; it is not really associated with
  // the origin, but rather with the individual capture-session.
  // (A capture-session can only be associated with a single origin,
  // but an origin might have multiple capture-sessions.)
}

}  // namespace permissions
