// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The JNI_OnLoad() definition for the linker library is moved here to avoid a
// conflict with JNI_Onload() defined by the test library. The linker tests
// together with the linker internals are smashed into (=linked with) the test
// library.
//
// This file also helps avoiding LegacyLinkerJNIInit() and its dependencies in
// base_unittests. There are no plans to unittest LegacyLinker.

#include <jni.h>

#include "base/android/linker/legacy_linker_jni.h"
#include "base/android/linker/linker_jni.h"
#include "base/android/linker/modern_linker_jni.h"

namespace chromium_android_linker {
namespace {

bool LinkerJNIInit(JavaVM* vm, JNIEnv* env) {
  // Find LibInfo field ids.
  LOG_INFO("Caching field IDs");
  if (!s_lib_info_fields.Init(env)) {
    return false;
  }

  return true;
}

}  // namespace

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
  if (!LinkerJNIInit(vm, env) || !LegacyLinkerJNIInit(vm, env) ||
      !ModernLinkerJNIInit(vm, env)) {
    return -1;
  }
  LOG_INFO("Done");
  return JNI_VERSION_1_4;
}

}  // namespace chromium_android_linker

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  return chromium_android_linker::JNI_OnLoad(vm, reserved);
}
