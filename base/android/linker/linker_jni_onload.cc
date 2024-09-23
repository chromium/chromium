// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The JNI_OnLoad() definition for the linker library is moved here to avoid a
// conflict with JNI_Onload() defined by the test library. The linker tests
// together with the linker internals are smashed into (=linked with) the test
// library.

#include <jni.h>

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android/linker/linker_jni.h"

namespace chromium_android_linker {

// JNI_OnLoad() is called when the linker library is loaded through the regular
// System.LoadLibrary) API. This shall save the Java VM handle and initialize
// LibInfo field accessors.
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  LOG_INFO("Entering");
  JNIEnv* env;
  if (JNI_OK != vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4)) {
    LOG_ERROR("Could not create JNIEnv");
    return -1;
  }
  if (!LinkerJNIInit(vm, env))
    return -1;
  LOG_INFO("Done");
  return JNI_VERSION_1_4;
}

}  // namespace chromium_android_linker

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  return chromium_android_linker::JNI_OnLoad(vm, reserved);
}
