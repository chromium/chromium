// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordSettingsUpdaterBridge_jni.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using password_manager::PasswordManagerSetting;
using SyncingAccount = PasswordSettingsUpdaterAndroidBridgeImpl::SyncingAccount;

namespace {

base::android::ScopedJavaLocalRef<jstring> GetJavaStringFromAccount(
    absl::optional<SyncingAccount> account) {
  if (!account.has_value()) {
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(), *account.value());
}

}  // namespace

namespace password_manager {

// static
bool PasswordSettingsUpdaterAndroidBridge::CanCreateAccessor() {
  return Java_PasswordSettingsUpdaterBridge_canCreateAccessor(
      base::android::AttachCurrentThread());
}

// static
std::unique_ptr<PasswordSettingsUpdaterAndroidBridge>
PasswordSettingsUpdaterAndroidBridge::Create() {
  DCHECK(Java_PasswordSettingsUpdaterBridge_canCreateAccessor(
      base::android::AttachCurrentThread()));
  return std::make_unique<PasswordSettingsUpdaterAndroidBridgeImpl>();
}

}  // namespace password_manager

PasswordSettingsUpdaterAndroidBridgeImpl::
    PasswordSettingsUpdaterAndroidBridgeImpl() {
  DCHECK(Java_PasswordSettingsUpdaterBridge_canCreateAccessor(
      base::android::AttachCurrentThread()));
  java_object_ = Java_PasswordSettingsUpdaterBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

PasswordSettingsUpdaterAndroidBridgeImpl::
    ~PasswordSettingsUpdaterAndroidBridgeImpl() {
  Java_PasswordSettingsUpdaterBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordSettingsUpdaterAndroidBridgeImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  consumer_ = std::move(consumer);
}

void PasswordSettingsUpdaterAndroidBridgeImpl::GetPasswordSettingValue(
    absl::optional<SyncingAccount> account,
    PasswordManagerSetting setting) {
  Java_PasswordSettingsUpdaterBridge_getSettingValue(
      base::android::AttachCurrentThread(), java_object_,
      GetJavaStringFromAccount(account), static_cast<int>(setting));
}

void PasswordSettingsUpdaterAndroidBridgeImpl::SetPasswordSettingValue(
    absl::optional<SyncingAccount> account,
    PasswordManagerSetting setting,
    bool value) {
  Java_PasswordSettingsUpdaterBridge_setSettingValue(
      base::android::AttachCurrentThread(), java_object_,
      GetJavaStringFromAccount(account), static_cast<int>(setting), value);
}

void PasswordSettingsUpdaterAndroidBridgeImpl::OnSettingValueFetched(
    JNIEnv* env,
    jint setting,
    jboolean setting_value) {
  if (!consumer_)
    return;
  consumer_->OnSettingValueFetched(static_cast<PasswordManagerSetting>(setting),
                                   setting_value);
}

void PasswordSettingsUpdaterAndroidBridgeImpl::OnSettingValueAbsent(
    JNIEnv* env,
    jint setting) {
  if (!consumer_)
    return;
  consumer_->OnSettingValueAbsent(static_cast<PasswordManagerSetting>(setting));
}

void PasswordSettingsUpdaterAndroidBridgeImpl::OnSettingFetchingError(
    JNIEnv* env,
    jint setting,
    jint error,
    jint api_error_code) {
  // TODO(crbug.com/1289700): Notify a consumer/record metrics.
}

void PasswordSettingsUpdaterAndroidBridgeImpl::OnSuccessfulSettingChange(
    JNIEnv* env,
    jint setting) {
  // TODO(crbug.com/1289700): Notify a consumer/record metrics.
}

void PasswordSettingsUpdaterAndroidBridgeImpl::OnFailedSettingChange(
    JNIEnv* env,
    jint setting,
    jint error,
    jint api_error_code) {
  // TODO(crbug.com/1289700): Notify a consumer/record metrics.
}
