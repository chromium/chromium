// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_id.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/values.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(
      content::BrowserContext* browser_context);

  ProtectedMediaIdentifierPermissionContext(
      const ProtectedMediaIdentifierPermissionContext&) = delete;
  ProtectedMediaIdentifierPermissionContext& operator=(
      const ProtectedMediaIdentifierPermissionContext&) = delete;

  ~ProtectedMediaIdentifierPermissionContext() override;

  // PermissionContextBase implementation.
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  // Returns whether "Protected content" is enabled based on factors other than
  // what 'ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER' is set to. For
  // example, it can be disabled by a switch in the content settings page, in
  // incognito or guest mode, or by the device policy.
  static bool IsProtectedMediaIdentifierEnabled(Profile* profile = nullptr);

 private:
  friend class ProtectedMediaIdentifierPermissionContextTest;
  static bool IsOriginAllowed(const GURL& origin);

  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnAttestationEnabledChanged(base::Value value);
  // We synchronize this property with ash-chrome so that we can check it
  // synchronously and not disturb the existing flow here.
  std::unique_ptr<CrosapiPrefObserver> attestation_enabled_observer_;
  inline static bool attestation_enabled_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
