// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_MICROPHONE_SYSTEM_PERMISSION_DELEGATE_MAC_H_
#define CHROME_BROWSER_PERMISSIONS_MICROPHONE_SYSTEM_PERMISSION_DELEGATE_MAC_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/permissions/system_permission_delegate.h"

class MicrophoneSystemPermissionDelegateMac
    : public EmbeddedPermissionPrompt::SystemPermissionDelegate {
 public:
  MicrophoneSystemPermissionDelegateMac();
  ~MicrophoneSystemPermissionDelegateMac() override;
  explicit MicrophoneSystemPermissionDelegateMac(
      const SystemPermissionDelegate&) = delete;
  MicrophoneSystemPermissionDelegateMac& operator=(
      const SystemPermissionDelegate&) = delete;

  // EmbeddedPermissionPrompt::SystemPermissionDelegate implementation.
  bool CanShowSystemPermissionPrompt() override;
  void RequestSystemPermission(
      SystemPermissionResponseCallback callback) override;
  void ShowSystemPermissionSettingsView() override;
  bool IsSystemPermissionDenied() override;
  bool IsSystemPermissionAllowed() override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_MICROPHONE_SYSTEM_PERMISSION_DELEGATE_MAC_H_
