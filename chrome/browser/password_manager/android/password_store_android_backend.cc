// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include <jni.h>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

using TaskId = PasswordStoreAndroidBackendBridge::TaskId;

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge)
    : bridge_(std::move(bridge)) {}

PasswordStoreAndroidBackend::~PasswordStoreAndroidBackend() = default;

void PasswordStoreAndroidBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  remote_form_changes_received_ = std::move(remote_form_changes_received);
  // TODO(https://crbug.com/1229650): Create subscription before completion.
  std::move(completion).Run(/*success=*/true);
}

void PasswordStoreAndroidBackend::GetAllLoginsAsync(LoginsReply callback) {
  // TODO(https://crbug.com/1229650): Implement by mapping task ID to callback.
  ignore_result(bridge_->GetAllLogins());
}

void PasswordStoreAndroidBackend::GetAutofillableLoginsAsync(
    LoginsReply callback) {
  // TODO(https://crbug.com/1229654): Implement.
}

void PasswordStoreAndroidBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    const std::vector<PasswordFormDigest>& forms) {
  // TODO(https://crbug.com/1229654): Implement.
}

void PasswordStoreAndroidBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::OnCompleteWithLogins(
    TaskId task_id,
    std::vector<PasswordForm> passwords) {
  // TODO(https://crbug.com/1229654): Implement.
}

SmartBubbleStatsStore* PasswordStoreAndroidBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

}  // namespace password_manager
