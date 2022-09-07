// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_generator/sample_jni_registration.h"
#include "base/android/jni_utils.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // By default, all JNI methods are registered. However, since render processes
  // don't need very much Java code, we enable selective JNI registration on the
  // Java side and only register a subset of JNI methods.
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!base::android::IsSelectiveJniRegistrationEnabled(env)) {
    if (!RegisterNonMainDexNatives(env)) {
      return -1;
    }
  }

  if (!RegisterMainDexNatives(env)) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
