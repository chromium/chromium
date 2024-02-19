// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_security_delegate.h"

#include "ash/public/cpp/app_types_util.h"
#include "components/exo/shell_surface_util.h"

namespace ash {

ChromeSecurityDelegate::ChromeSecurityDelegate() = default;

ChromeSecurityDelegate::~ChromeSecurityDelegate() = default;

bool ChromeSecurityDelegate::CanSelfActivate(aura::Window* window) const {
  // TODO(b/233691818): The default should be "false", and clients should
  // override that if they need to self-activate.
  //
  // Unfortunately, several clients don't have their own SecurityDelegate yet,
  // so we will continue to use the old exo::Permissions stuff until they do.
  return exo::HasPermissionToActivate(window);
}

bool ChromeSecurityDelegate::CanLockPointer(aura::Window* window) const {
  // TODO(b/200896773): Move this out from exo's default security delegate
  // define in client's security delegates.
  return ash::IsArcWindow(window) || ash::IsLacrosWindow(window);
}

ChromeSecurityDelegate::SetBoundsPolicy ChromeSecurityDelegate::CanSetBounds(
    aura::Window* window) const {
  // TODO(b/200896773): Move into LacrosSecurityDelegate when it exists.
  if (ash::IsLacrosWindow(window)) {
    return SetBoundsPolicy::DCHECK_IF_DECORATED;
  } else if (ash::IsArcWindow(window)) {
    // TODO(b/285252684): Move into ArcSecurityDelegate when it exists.
    return SetBoundsPolicy::ADJUST_IF_DECORATED;
  } else {
    return SetBoundsPolicy::IGNORE;
  }
}

}  // namespace ash
