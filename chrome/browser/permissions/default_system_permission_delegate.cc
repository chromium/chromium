// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/default_system_permission_delegate.h"

bool DefaultSystemPermissionDelegate::CanShowSystemPermissionPrompt() {
  return false;
}

void DefaultSystemPermissionDelegate::RequestSystemPermission(
    SystemPermissionResponseCallback callback) {
  std::move(callback).Run();
}

void DefaultSystemPermissionDelegate::ShowSystemPermissionSettingsView() {}

bool DefaultSystemPermissionDelegate::IsSystemPermissionDenied() {
  return false;
}

bool DefaultSystemPermissionDelegate::IsSystemPermissionAllowed() {
  return false;
}
