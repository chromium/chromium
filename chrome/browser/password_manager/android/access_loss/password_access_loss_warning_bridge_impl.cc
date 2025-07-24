// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"

#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"

PasswordAccessLossWarningBridgeImpl::PasswordAccessLossWarningBridgeImpl() =
    default;

PasswordAccessLossWarningBridgeImpl::~PasswordAccessLossWarningBridgeImpl() =
    default;

bool PasswordAccessLossWarningBridgeImpl::ShouldShowAccessLossNoticeSheet(
    PrefService* pref_service,
    bool called_at_startup) {
  return false;
}

void PasswordAccessLossWarningBridgeImpl::MaybeShowAccessLossNoticeSheet(
    PrefService* pref_service,
    const gfx::NativeWindow window,
    Profile* profile,
    bool called_at_startup,
    password_manager_android_util::PasswordAccessLossWarningTriggers
        trigger_source) {}

void PasswordAccessLossWarningBridgeImpl::SetUtilBridgeForTesting(
    std::unique_ptr<
        password_manager_android_util::PasswordManagerUtilBridgeInterface>
        util_bridge) {
  CHECK(!util_bridge_);
  util_bridge_ = std::move(util_bridge);
}

password_manager_android_util::PasswordManagerUtilBridgeInterface&
PasswordAccessLossWarningBridgeImpl::GetUtilBridge() {
  if (!util_bridge_) {
    util_bridge_ = std::make_unique<
        password_manager_android_util::PasswordManagerUtilBridge>();
  }
  return *util_bridge_;
}
