// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"

#include <jni.h>
#include <cstdint>
#include <utility>

#include "base/android/jni_android.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerLifecycleHelper_jni.h"

PasswordManagerLifecycleHelper::PasswordManagerLifecycleHelper(
    base::RepeatingClosure foregrounding_callback) {
  java_object_ = Java_PasswordManagerLifecycleHelper_getInstance(
      base::android::AttachCurrentThread());
  foregrounding_callback_ = std::move(foregrounding_callback);
  Java_PasswordManagerLifecycleHelper_registerObserver(
      base::android::AttachCurrentThread(), java_object_,
      reinterpret_cast<intptr_t>(this));
}

PasswordManagerLifecycleHelper::~PasswordManagerLifecycleHelper() {
  Java_PasswordManagerLifecycleHelper_unregisterObserver(
      base::android::AttachCurrentThread(), java_object_,
      reinterpret_cast<intptr_t>(this));
}

void PasswordManagerLifecycleHelper::OnForegroundSessionStart(JNIEnv* env) {
  DCHECK(foregrounding_callback_);
  foregrounding_callback_.Run();
}
