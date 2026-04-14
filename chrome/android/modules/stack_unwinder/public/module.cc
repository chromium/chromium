// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/modules/stack_unwinder/public/module.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/modules/stack_unwinder/provider/jni_headers/StackUnwinderModuleProvider_jni.h"

namespace stack_unwinder {

// static
bool Module::IsInstalled() {
  if (base::android::IsJavaAvailable()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    return Java_StackUnwinderModuleProvider_isModuleInstalled(env);
  } else {
    return false;
  }
}

// static
void Module::RequestInstallation() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_StackUnwinderModuleProvider_installModule(env);
}

// static
std::unique_ptr<Module> Module::Load() {
  CHECK(IsInstalled());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_StackUnwinderModuleProvider_ensureNativeLoaded(env);

  return base::WrapUnique(new Module());
}

}  // namespace stack_unwinder

DEFINE_JNI(StackUnwinderModuleProvider)
