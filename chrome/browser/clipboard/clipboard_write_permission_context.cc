// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard/clipboard_write_permission_context.h"

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"
#include "url/gurl.h"

ClipboardWritePermissionContext::ClipboardWritePermissionContext(
    Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::CLIPBOARD_WRITE,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

ClipboardWritePermissionContext::~ClipboardWritePermissionContext() {}

ContentSetting ClipboardWritePermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_ALLOW;
}

bool ClipboardWritePermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
