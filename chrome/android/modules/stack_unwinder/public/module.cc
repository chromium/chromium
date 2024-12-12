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
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_StackUnwinderModuleProvider_isModuleInstalled(env);
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

  DoNothingFunction do_nothing = reinterpret_cast<DoNothingFunction>(
      Java_StackUnwinderModuleProvider_getDoNothingFunction(env));

  return base::WrapUnique(new Module(do_nothing));
}

void Module::DoNothing() {
  return do_nothing_();
}

Module::Module(DoNothingFunction do_nothing) : do_nothing_(do_nothing) {
  DCHECK(do_nothing);
}

}  // namespace stack_unwinder
