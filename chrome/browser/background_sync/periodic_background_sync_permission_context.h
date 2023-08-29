// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_context_base.h"

namespace content {
class BrowserContext;
}

namespace features {

BASE_DECLARE_FEATURE(kPeriodicSyncPermissionForDefaultSearchEngine);

}  // namespace features

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
class PeriodicBackgroundSyncPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit PeriodicBackgroundSyncPermissionContext(
      content::BrowserContext* browser_context);

  PeriodicBackgroundSyncPermissionContext(
      const PeriodicBackgroundSyncPermissionContext&) = delete;
  PeriodicBackgroundSyncPermissionContext& operator=(
      const PeriodicBackgroundSyncPermissionContext&) = delete;

  ~PeriodicBackgroundSyncPermissionContext() override;

 protected:
  // Virtual for testing.
  virtual bool IsPwaInstalled(const GURL& origin) const;
#if BUILDFLAG(IS_ANDROID)
  virtual bool IsTwaInstalled(const GURL& origin) const;
#endif
  virtual GURL GetDefaultSearchEngineUrl() const;

 private:
  // PermissionContextBase implementation.
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;
  void NotifyPermissionSet(const permissions::PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           permissions::BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time,
                           bool is_final_decision) override;
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_PERMISSION_CONTEXT_H_
