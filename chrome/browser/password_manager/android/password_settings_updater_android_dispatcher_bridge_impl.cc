// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge_impl.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordSettingsUpdaterDispatcherBridge_jni.h"

namespace password_manager {

namespace {

using SyncingAccount =
    PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::SyncingAccount;

base::android::ScopedJavaLocalRef<jstring> GetJavaStringFromAccount(
    std::optional<SyncingAccount> account) {
  if (!account.has_value()) {
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(), *account.value());
}

}  // namespace

// static
std::unique_ptr<PasswordSettingsUpdaterAndroidDispatcherBridge>
PasswordSettingsUpdaterAndroidDispatcherBridge::Create() {
  CHECK(password_manager_android_util::AreMinUpmRequirementsMet());
  return std::make_unique<PasswordSettingsUpdaterAndroidDispatcherBridgeImpl>();
}

PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    PasswordSettingsUpdaterAndroidDispatcherBridgeImpl() {
  DETACH_FROM_THREAD(thread_checker_);
  CHECK(password_manager_android_util::AreMinUpmRequirementsMet());
}

PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    ~PasswordSettingsUpdaterAndroidDispatcherBridgeImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::Init(
    base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  java_object_ = Java_PasswordSettingsUpdaterDispatcherBridge_create(
      base::android::AttachCurrentThread(), receiver_bridge);
}

void PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    GetPasswordSettingValue(std::optional<SyncingAccount> account,
                            PasswordManagerSetting setting) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordSettingsUpdaterDispatcherBridge_getSettingValue(
      base::android::AttachCurrentThread(), java_object_,
      GetJavaStringFromAccount(account), static_cast<int>(setting));
}

void PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    SetPasswordSettingValue(std::optional<SyncingAccount> account,
                            PasswordManagerSetting setting,
                            bool value) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordSettingsUpdaterDispatcherBridge_setSettingValue(
      base::android::AttachCurrentThread(), java_object_,
      GetJavaStringFromAccount(account), static_cast<int>(setting), value);
}

}  // namespace password_manager
