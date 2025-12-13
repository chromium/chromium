// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_PERMISSION_CONTEXT_H_

#include "components/permissions/content_setting_permission_context_base.h"

class WebPrintingPermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  explicit WebPrintingPermissionContext(
      content::BrowserContext* browser_context);
  ~WebPrintingPermissionContext() override;

 private:
  // ContentSettingPermissionContextBase:
  void UpdateTabContext(const permissions::PermissionRequestData& request_data,
                        bool allowed) override;
};

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_PERMISSION_CONTEXT_H_
