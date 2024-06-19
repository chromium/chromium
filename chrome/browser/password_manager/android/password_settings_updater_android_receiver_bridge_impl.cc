// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge_impl.h"

#include "base/android/jni_android.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordSettingsUpdaterReceiverBridge_jni.h"

namespace password_manager {

namespace {

using SyncingAccount =
    PasswordSettingsUpdaterAndroidReceiverBridgeImpl::SyncingAccount;

}

// static
std::unique_ptr<PasswordSettingsUpdaterAndroidReceiverBridge>
PasswordSettingsUpdaterAndroidReceiverBridge::Create() {
  return std::make_unique<PasswordSettingsUpdaterAndroidReceiverBridgeImpl>();
}

PasswordSettingsUpdaterAndroidReceiverBridgeImpl::
    PasswordSettingsUpdaterAndroidReceiverBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  java_object_ = Java_PasswordSettingsUpdaterReceiverBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

PasswordSettingsUpdaterAndroidReceiverBridgeImpl::
    ~PasswordSettingsUpdaterAndroidReceiverBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  Java_PasswordSettingsUpdaterReceiverBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

base::android::ScopedJavaGlobalRef<jobject>
PasswordSettingsUpdaterAndroidReceiverBridgeImpl::GetJavaBridge() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  return java_object_;
}

void PasswordSettingsUpdaterAndroidReceiverBridgeImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  consumer_ = std::move(consumer);
}

void PasswordSettingsUpdaterAndroidReceiverBridgeImpl::OnSettingValueFetched(
    JNIEnv* env,
    jint setting,
    jboolean setting_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!consumer_) {
    return;
  }
  consumer_->OnSettingValueFetched(static_cast<PasswordManagerSetting>(setting),
                                   setting_value);
}

void PasswordSettingsUpdaterAndroidReceiverBridgeImpl::OnSettingValueAbsent(
    JNIEnv* env,
    jint setting) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!consumer_) {
    return;
  }
  consumer_->OnSettingValueAbsent(static_cast<PasswordManagerSetting>(setting));
}

void PasswordSettingsUpdaterAndroidReceiverBridgeImpl::OnSettingFetchingError(
    JNIEnv* env,
    jint setting,
    jint error,
    jint api_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  consumer_->OnSettingFetchingError(
      static_cast<PasswordManagerSetting>(setting),
      static_cast<AndroidBackendAPIErrorCode>(api_error_code));
}

void PasswordSettingsUpdaterAndroidReceiverBridgeImpl::
    OnSuccessfulSettingChange(JNIEnv* env, jint setting) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // TODO(crbug.com/40212062): Record metrics.
  consumer_->OnSuccessfulSettingChange(
      static_cast<PasswordManagerSetting>(setting));
}

void PasswordSettingsUpdaterAndroidReceiverBridgeImpl::OnFailedSettingChange(
    JNIEnv* env,
    jint setting,
    jint error,
    jint api_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  consumer_->OnFailedSettingChange(
      static_cast<PasswordManagerSetting>(setting),
      static_cast<AndroidBackendAPIErrorCode>(api_error_code));
}

}  // namespace password_manager
