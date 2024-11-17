// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_IMPL_H_

#include <jni.h>

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge.h"

class PasswordAccessLossWarningBridgeImpl
    : public PasswordAccessLossWarningBridge {
 public:
  PasswordAccessLossWarningBridgeImpl();
  PasswordAccessLossWarningBridgeImpl(
      const PasswordAccessLossWarningBridgeImpl&) = delete;
  PasswordAccessLossWarningBridgeImpl& operator=(
      const PasswordAccessLossWarningBridgeImpl&) = delete;
  ~PasswordAccessLossWarningBridgeImpl() override;

  bool ShouldShowAccessLossNoticeSheet(PrefService* pref_service,
                                       bool called_at_startup) override;
  void MaybeShowAccessLossNoticeSheet(
      PrefService* pref_service,
      const gfx::NativeWindow window,
      Profile* profile,
      bool called_at_startup,
      password_manager_android_util::PasswordAccessLossWarningTriggers
          trigger_source) override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_IMPL_H_
