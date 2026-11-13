// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PWC_CONTENT_SETTINGS_POLICY_H_
#define CHROME_BROWSER_PWC_PWC_CONTENT_SETTINGS_POLICY_H_

#include "components/content_settings/core/common/content_settings_types.h"

namespace pwc {

// Classification of how a ContentSettingsType behaves inside a
// PrivilegedWebContents.
//
// PrivilegedWebContents runs in the user profile's default StoragePartition so
// that first-party components share live profile auth cookies and user
// sessions. However, ambient user site settings configured on the capability
// origin in regular tabs must never unexpectedly leak into or break the
// PrivilegedWebContents.
//
// Note: This policy currently governs Web platform permissions evaluated
// through PermissionManager (via PrivilegedWebContents::GetPermissionStatus).
// Non-PermissionManager subsystems (such as device chooser delegates for
// USB/HID/Serial and direct HostContentSettingsMap queries) do not currently
// route through GetPermissionStatus; kBlocked serves as an audit classification
// for those types until chooser delegates are wired up to PWC.
enum class PwcContentSettingPolicy {
  // Web platform permissions handled and overridden by the component's
  // PwcPermissionDelegate (via PrivilegedWebContents::GetPermissionStatus). If
  // unhandled by the delegate, GetPermissionStatus() defaults to DENIED with
  // FEATURE_POLICY.
  kDelegated,

  // Blocked / denied in PrivilegedWebContents::GetPermissionStatus().
  kBlocked,
};

// Returns the PwcContentSettingPolicy governing `type` in a
// PrivilegedWebContents.
PwcContentSettingPolicy GetPwcContentSettingPolicy(ContentSettingsType type);

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PWC_CONTENT_SETTINGS_POLICY_H_
