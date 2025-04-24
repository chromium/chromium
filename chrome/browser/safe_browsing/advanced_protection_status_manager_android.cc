// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_android.h"

#include <jni.h>

#include "base/command_line.h"
#include "chrome/browser/safe_browsing/android/jni_headers/AdvancedProtectionStatusManagerAndroidBridge_jni.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing {

AdvancedProtectionStatusManagerAndroid::
    AdvancedProtectionStatusManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_manager_ = Java_AdvancedProtectionStatusManagerAndroidBridge_create(
      env, reinterpret_cast<intptr_t>(this));
  UpdateState();
}

AdvancedProtectionStatusManagerAndroid::
    ~AdvancedProtectionStatusManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AdvancedProtectionStatusManagerAndroidBridge_destroy(env, java_manager_);
}

// static
bool AdvancedProtectionStatusManagerAndroid::QueryIsUnderAdvancedProtection() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AdvancedProtectionStatusManagerAndroidBridge_isUnderAdvancedProtection(
      env);
}

bool AdvancedProtectionStatusManagerAndroid::IsUnderAdvancedProtection() const {
  return is_under_advanced_protection_;
}

void AdvancedProtectionStatusManagerAndroid::
    SetAdvancedProtectionStatusForTesting(bool enrolled) {
  if (is_under_advanced_protection_ == enrolled) {
    return;
  }

  is_under_advanced_protection_ = enrolled;
  NotifyObserversStatusChanged();
}

void AdvancedProtectionStatusManagerAndroid::
    OnAdvancedProtectionOsSettingChanged(JNIEnv* env) {
  bool was_under_advanced_protection = is_under_advanced_protection_;
  UpdateState();
  if (was_under_advanced_protection == is_under_advanced_protection_) {
    return;
  }

  NotifyObserversStatusChanged();
}

void AdvancedProtectionStatusManagerAndroid::UpdateState() {
  // TODO(crbug.com/412761437) Command line switch presence is tested in both
  // C++ and Java. Remove duplicate switch presence switch.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceTreatUserAsAdvancedProtection)) {
    is_under_advanced_protection_ = true;
    return;
  }
  is_under_advanced_protection_ =
      AdvancedProtectionStatusManagerAndroid::QueryIsUnderAdvancedProtection();
}

}  // namespace safe_browsing
