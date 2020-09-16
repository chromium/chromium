// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IDLE_IDLE_DETECTION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_IDLE_IDLE_DETECTION_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/permissions/permission_context_base.h"

class IdleDetectionPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit IdleDetectionPermissionContext(
      content::BrowserContext* browser_context);
  ~IdleDetectionPermissionContext() override;

 private:
  // PermissionContextBase:
  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
  bool IsRestrictedToSecureOrigins() const override;
  void DecidePermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;

  base::WeakPtrFactory<IdleDetectionPermissionContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IdleDetectionPermissionContext);
};

#endif  // CHROME_BROWSER_IDLE_IDLE_DETECTION_PERMISSION_CONTEXT_H_
