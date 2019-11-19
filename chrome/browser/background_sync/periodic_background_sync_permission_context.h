// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "components/content_settings/core/common/content_settings.h"

class Profile;

// This permission context is responsible for getting, deciding on and updating
// the Periodic Background Sync permission for a particular website. This
// permission guards the use of the Periodic Background Sync API. It's not being
// persisted because it's dynamic and relies on either the presence of a PWA for
// the origin, and the Periodic and one-shot Background Sync content settings.
// The user is never prompted for this permission. They can disable the feature
// by disabling the (one-shot) Background Sync permission from content settings
// UI. The periodic Background Sync content setting can be disabled via Finch,
// and will prevent usage of the API.
// The permission decision is made as follows:
// If the feature is disabled, deny.
// If we're on Android, and there's a TWA installed for the origin, grant
// permission.
// For other platforms, if there's no PWA installed for the origin, deny.
// If there is a PWA installed, grant/deny permission based on whether the
// one-shot Background Sync content setting is set to allow/block.
class PeriodicBackgroundSyncPermissionContext : public PermissionContextBase {
 public:
  explicit PeriodicBackgroundSyncPermissionContext(Profile* profile);
  ~PeriodicBackgroundSyncPermissionContext() override;

 protected:
  // Virtual for testing.
  virtual bool IsPwaInstalled(const GURL& url) const;
#if defined(OS_ANDROID)
  virtual bool IsTwaInstalled(const GURL& url) const;
#endif

 private:
  // PermissionContextBase implementation.
  bool IsRestrictedToSecureOrigins() const override;
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        BrowserPermissionCallback callback) override;
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting) override;

  DISALLOW_COPY_AND_ASSIGN(PeriodicBackgroundSyncPermissionContext);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
