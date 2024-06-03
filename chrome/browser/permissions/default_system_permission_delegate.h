// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_DEFAULT_SYSTEM_PERMISSION_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_DEFAULT_SYSTEM_PERMISSION_DELEGATE_H_

#include "chrome/browser/permissions/system_permission_delegate.h"

// The DefaultSystemPermissionDelegate can be used when there is no system
// permission requirement for a given permission type.
class DefaultSystemPermissionDelegate
    : public EmbeddedPermissionPrompt::SystemPermissionDelegate {
 public:
  DefaultSystemPermissionDelegate() = default;
  ~DefaultSystemPermissionDelegate() override = default;
  explicit DefaultSystemPermissionDelegate(const SystemPermissionDelegate&) =
      delete;
  DefaultSystemPermissionDelegate& operator=(const SystemPermissionDelegate&) =
      delete;

  // EmbeddedPermissionPrompt::SystemPermissionDelegate implementation.
  bool CanShowSystemPermissionPrompt() override;
  void RequestSystemPermission(
      SystemPermissionResponseCallback callback) override;
  void ShowSystemPermissionSettingsView() override;
  bool IsSystemPermissionDenied() override;
  bool IsSystemPermissionAllowed() override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_DEFAULT_SYSTEM_PERMISSION_DELEGATE_H_
