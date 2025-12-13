// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include "chrome/browser/profiles/profile.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_request_id.h"

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(
      content::BrowserContext* browser_context);

  ProtectedMediaIdentifierPermissionContext(
      const ProtectedMediaIdentifierPermissionContext&) = delete;
  ProtectedMediaIdentifierPermissionContext& operator=(
      const ProtectedMediaIdentifierPermissionContext&) = delete;

  ~ProtectedMediaIdentifierPermissionContext() override;

  // ContentSettingPermissionContextBase implementation.
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  // Returns whether "Protected content" is enabled based on factors other than
  // what 'ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER' is set to. For
  // example, it can be disabled by a switch in the content settings page, in
  // incognito or guest mode, or by the device policy.
  static bool IsProtectedMediaIdentifierEnabled(Profile* profile = nullptr);

 private:
  friend class ProtectedMediaIdentifierPermissionContextTest;
  static bool IsOriginAllowed(const GURL& origin);

  void UpdateTabContext(const permissions::PermissionRequestData& request_data,
                        bool allowed) override;
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
