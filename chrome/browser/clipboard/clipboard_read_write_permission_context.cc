// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard/clipboard_read_write_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

ClipboardReadWritePermissionContext::ClipboardReadWritePermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::CLIPBOARD_READ_WRITE,
          blink::mojom::FeaturePolicyFeature::kClipboardRead) {}

ClipboardReadWritePermissionContext::~ClipboardReadWritePermissionContext() {}

void ClipboardReadWritePermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.render_process_id(), id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnContentAllowed(
        ContentSettingsType::CLIPBOARD_READ_WRITE);
  } else {
    content_settings->OnContentBlocked(
        ContentSettingsType::CLIPBOARD_READ_WRITE);
  }
}

bool ClipboardReadWritePermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
