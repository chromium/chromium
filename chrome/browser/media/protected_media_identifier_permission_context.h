// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_request_id.h"

#if defined(OS_CHROMEOS)
#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#endif

class Profile;

namespace views {
class Widget;
}

namespace content {
class WebContents;
}

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public PermissionContextBase {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(Profile* profile);
  ~ProtectedMediaIdentifierPermissionContext() override;

  // PermissionContextBase implementation.
#if defined(OS_CHROMEOS)
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin,
                        bool user_gesture,
                        BrowserPermissionCallback callback) override;
#endif  // defined(OS_CHROMEOS)
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 private:
  friend class ProtectedMediaIdentifierPermissionContextTest;
  static bool IsOriginWhitelisted(const GURL& origin);

  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
  bool IsRestrictedToSecureOrigins() const override;

  // Returns whether "Protected content" is enabled based on factors other
  // than the protected media identifier content setting itself. For example,
  // it can be disabled by a master switch in content settings, in incognito or
  // guest mode, or by the device policy.
  bool IsProtectedMediaIdentifierEnabled() const;

#if defined(OS_CHROMEOS)
  void OnPlatformVerificationConsentResponse(
      content::WebContents* web_contents,
      const PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      BrowserPermissionCallback callback,
      chromeos::attestation::PlatformVerificationDialog::ConsentResponse
          response);

  // |this| is shared among multiple WebContents, so we could receive multiple
  // permission requests. This map tracks all pending requests. Note that we
  // only allow one request per WebContents.
  typedef std::map<content::WebContents*,
                   std::pair<views::Widget*, PermissionRequestID>>
      PendingRequestMap;
  PendingRequestMap pending_requests_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<ProtectedMediaIdentifierPermissionContext> weak_factory_{
      this};
#endif

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
