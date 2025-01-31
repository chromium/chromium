// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

WebPrintingPermissionContext::WebPrintingPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::WEB_PRINTING,
          blink::mojom::PermissionsPolicyFeature::kWebPrinting) {}

WebPrintingPermissionContext::~WebPrintingPermissionContext() = default;

void WebPrintingPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  if (auto* content_settings =
          content_settings::PageSpecificContentSettings::GetForFrame(
              id.global_render_frame_host_id())) {
    if (allowed) {
      content_settings->OnContentAllowed(ContentSettingsType::WEB_PRINTING);
    } else {
      content_settings->OnContentBlocked(ContentSettingsType::WEB_PRINTING);
    }
  }
}
