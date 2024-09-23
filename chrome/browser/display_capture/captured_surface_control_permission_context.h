// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DISPLAY_CAPTURE_CAPTURED_SURFACE_CONTROL_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_DISPLAY_CAPTURE_CAPTURED_SURFACE_CONTROL_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class CapturedSurfaceControlPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit CapturedSurfaceControlPermissionContext(
      content::BrowserContext* browser_context);
  ~CapturedSurfaceControlPermissionContext() override = default;

  CapturedSurfaceControlPermissionContext(
      const CapturedSurfaceControlPermissionContext&) = delete;
  CapturedSurfaceControlPermissionContext& operator=(
      const CapturedSurfaceControlPermissionContext&) = delete;

  bool UsesAutomaticEmbargo() const override;

 protected:
  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting,
                            bool is_one_time) override;

 private:
  const bool sticky_permissions_;
};

}  // namespace permissions

#endif  // CHROME_BROWSER_DISPLAY_CAPTURE_CAPTURED_SURFACE_CONTROL_PERMISSION_CONTEXT_H_
