// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_

#include "base/gtest_prod_util.h"
#include "components/permissions/permission_context_base.h"

extern const int kDefaultImplicitGrantLimit;

class StorageAccessGrantPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit StorageAccessGrantPermissionContext(
      content::BrowserContext* browser_context);

  StorageAccessGrantPermissionContext(
      const StorageAccessGrantPermissionContext&) = delete;
  StorageAccessGrantPermissionContext& operator=(
      const StorageAccessGrantPermissionContext&) = delete;

  ~StorageAccessGrantPermissionContext() override;

  // Exposes `DecidePermission` for tests.
  void DecidePermissionForTesting(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback);

 private:
  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;
  void DecidePermission(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void NotifyPermissionSet(const permissions::PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           permissions::BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time) override;
  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting,
                            bool is_one_time) override;

  // Internal implementation for NotifyPermissionSet. Allows for differentiation
  // of implicit and explicit grants using |implicit_result|.
  void NotifyPermissionSetInternal(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      permissions::BrowserPermissionCallback callback,
      bool persist,
      ContentSetting content_setting,
      bool implicit_result);
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_
