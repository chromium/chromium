// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NFC_NFC_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NFC_NFC_PERMISSION_CONTEXT_H_

#include "chrome/browser/permissions/permission_context_base.h"

class NfcPermissionContext : public PermissionContextBase {
 public:
  explicit NfcPermissionContext(Profile* profile);

  NfcPermissionContext(const NfcPermissionContext&) = delete;
  NfcPermissionContext& operator=(const NfcPermissionContext&) = delete;

  ~NfcPermissionContext() override;

 private:
  // PermissionContextBase:
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
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
  bool IsRestrictedToSecureOrigins() const override;
};

#endif  // CHROME_BROWSER_NFC_NFC_PERMISSION_CONTEXT_H_
