// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_COMMON_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_COMMON_H_

#include "base/functional/callback_forward.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace system_permission_settings {

// System permission state. These are also used in stats - do not remove or
// re-arrange the values.
enum class SystemPermission {
  kNotDetermined = 0,
  kRestricted = 1,
  kDenied = 2,
  kAllowed = 3,
  kMaxValue = kAllowed
};

inline bool IsSystemPermissionDenied(SystemPermission permission) {
  return permission == SystemPermission::kDenied ||
         permission == SystemPermission::kRestricted;
}

inline bool IsSystemPermissionAllowed(SystemPermission permission) {
  return permission == SystemPermission::kAllowed;
}

inline bool IsSystemPermissionPrompt(SystemPermission permission) {
  return permission == SystemPermission::kNotDetermined;
}

using SystemPermissionResponseCallback = base::OnceCallback<void()>;
using content_settings::mojom::ContentSettingsType;
using SystemPermissionChangedCallback =
    base::RepeatingCallback<void(ContentSettingsType /*type*/,
                                 bool /*is_blocked*/)>;
using SystemPermissionDeniedCallback = base::OnceCallback<void(bool)>;

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_PERMISSION_COMMON_H_
