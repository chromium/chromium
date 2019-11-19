// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard/clipboard_read_permission_context.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

ClipboardReadPermissionContext::ClipboardReadPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::CLIPBOARD_READ,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

ClipboardReadPermissionContext::~ClipboardReadPermissionContext() {}

void ClipboardReadPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::GetForFrame(id.render_process_id(),
                                              id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnContentAllowed(ContentSettingsType::CLIPBOARD_READ);
  } else {
    content_settings->OnContentBlocked(ContentSettingsType::CLIPBOARD_READ);
  }
}

bool ClipboardReadPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
