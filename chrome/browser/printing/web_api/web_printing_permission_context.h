// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class WebPrintingPermissionContext : public permissions::PermissionContextBase {
 public:
  explicit WebPrintingPermissionContext(
      content::BrowserContext* browser_context);
  ~WebPrintingPermissionContext() override;

 private:
  // PermissionContextBase:
  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
};

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_PERMISSION_CONTEXT_H_
