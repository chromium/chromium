// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"

#include <jni.h>

#include <cstdint>
#include <utility>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerLifecycleHelper_jni.h"

PasswordManagerLifecycleHelperImpl::PasswordManagerLifecycleHelperImpl() {
  java_object_ = Java_PasswordManagerLifecycleHelper_getInstance(
      base::android::AttachCurrentThread());
}

void PasswordManagerLifecycleHelperImpl::RegisterObserver(
    base::RepeatingClosure foregrounding_callback) {
  foregrounding_callback_ = std::move(foregrounding_callback);
  Java_PasswordManagerLifecycleHelper_registerObserver(
      base::android::AttachCurrentThread(), java_object_,
      reinterpret_cast<intptr_t>(this));
}

void PasswordManagerLifecycleHelperImpl::UnregisterObserver() {
  Java_PasswordManagerLifecycleHelper_unregisterObserver(
      base::android::AttachCurrentThread(), java_object_,
      reinterpret_cast<intptr_t>(this));
  foregrounding_callback_.Reset();
}

PasswordManagerLifecycleHelperImpl::~PasswordManagerLifecycleHelperImpl() {
  DCHECK(!foregrounding_callback_) << "Did not call UnregisterObserver!";
}

void PasswordManagerLifecycleHelperImpl::OnForegroundSessionStart(JNIEnv* env) {
  DCHECK(foregrounding_callback_);
  foregrounding_callback_.Run();
}
