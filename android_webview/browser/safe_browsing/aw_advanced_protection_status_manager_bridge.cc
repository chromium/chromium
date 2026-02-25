// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_advanced_protection_status_manager_bridge.h"

#include "android_webview/browser_jni_headers/AwAdvancedProtectionStatusManagerBridge_jni.h"
#include "base/android/jni_android.h"
#include "base/no_destructor.h"

namespace android_webview {

// static
AwAdvancedProtectionStatusManagerBridge*
AwAdvancedProtectionStatusManagerBridge::GetInstance() {
  static base::NoDestructor<AwAdvancedProtectionStatusManagerBridge> instance;
  return instance.get();
}

AwAdvancedProtectionStatusManagerBridge::
    AwAdvancedProtectionStatusManagerBridge() = default;

AwAdvancedProtectionStatusManagerBridge::
    ~AwAdvancedProtectionStatusManagerBridge() = default;

// static
bool AwAdvancedProtectionStatusManagerBridge::IsUnderAdvancedProtection() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AwAdvancedProtectionStatusManagerBridge_isUnderAdvancedProtection(
      env);
}

void AwAdvancedProtectionStatusManagerBridge::
    OnAdvancedProtectionOsSettingChanged() {
  for (auto& observer : observers_) {
    observer.OnAdvancedProtectionStatusChanged(IsUnderAdvancedProtection());
  }
}

static void
JNI_AwAdvancedProtectionStatusManagerBridge_OnAdvancedProtectionOsSettingChanged(
    JNIEnv* env) {
  AwAdvancedProtectionStatusManagerBridge::GetInstance()
      ->OnAdvancedProtectionOsSettingChanged();
}

void AwAdvancedProtectionStatusManagerBridge::AddObserver(Observer* observer) {
  if (observers_.empty()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwAdvancedProtectionStatusManagerBridge_startObserving(env);
  }
  observers_.AddObserver(observer);
  // Notify observer of current state.
  observer->OnAdvancedProtectionStatusChanged(IsUnderAdvancedProtection());
}

void AwAdvancedProtectionStatusManagerBridge::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwAdvancedProtectionStatusManagerBridge_stopObserving(env);
  }
}

}  // namespace android_webview

DEFINE_JNI(AwAdvancedProtectionStatusManagerBridge)
