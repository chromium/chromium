// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nfc/nfc_permission_context.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_request_id.h"
#if !defined(OS_ANDROID)
#error This file currently only supports Android.
#endif

NfcPermissionContext::NfcPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::NFC,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

NfcPermissionContext::~NfcPermissionContext() = default;

ContentSetting NfcPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_BLOCK;
}

void NfcPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize NFC from
  // NfcPermissionContext.
  NOTREACHED();
}

void NfcPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                            const GURL& requesting_frame,
                                            bool allowed) {
  auto* content_settings = TabSpecificContentSettings::GetForFrame(
      id.render_process_id(), id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed)
    content_settings->OnContentAllowed(ContentSettingsType::NFC);
  else
    content_settings->OnContentBlocked(ContentSettingsType::NFC);
}

bool NfcPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
