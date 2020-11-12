// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_FLASH_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PLUGINS_FLASH_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/permissions/permission_context_base.h"

class GURL;

namespace permissions {
class PermissionRequestID;
}

class FlashPermissionContext : public permissions::PermissionContextBase {
 public:
  explicit FlashPermissionContext(content::BrowserContext* browser_context);
  ~FlashPermissionContext() override;

 private:
  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool allowed) override;
  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting) override;
  bool IsRestrictedToSecureOrigins() const override;

  DISALLOW_COPY_AND_ASSIGN(FlashPermissionContext);
};

#endif  // CHROME_BROWSER_PLUGINS_FLASH_PERMISSION_CONTEXT_H_
