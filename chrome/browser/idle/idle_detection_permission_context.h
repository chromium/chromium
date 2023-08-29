// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IDLE_IDLE_DETECTION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_IDLE_IDLE_DETECTION_PERMISSION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "components/permissions/permission_context_base.h"

class IdleDetectionPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit IdleDetectionPermissionContext(
      content::BrowserContext* browser_context);

  IdleDetectionPermissionContext(const IdleDetectionPermissionContext&) =
      delete;
  IdleDetectionPermissionContext& operator=(
      const IdleDetectionPermissionContext&) = delete;

  ~IdleDetectionPermissionContext() override;

 private:
  // PermissionContextBase:
  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;

  base::WeakPtrFactory<IdleDetectionPermissionContext> weak_factory_{this};
};

#endif  // CHROME_BROWSER_IDLE_IDLE_DETECTION_PERMISSION_CONTEXT_H_
