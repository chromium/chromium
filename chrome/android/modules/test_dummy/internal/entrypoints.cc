// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/android/jni_utils.h"
#include "chrome/android/modules/test_dummy/internal/jni_registration.h"

extern "C" {
// This JNI registration method is found and called by module framework code.
JNI_GENERATOR_EXPORT bool JNI_OnLoad_test_dummy(JNIEnv* env) {
  if (!base::android::IsSelectiveJniRegistrationEnabled(env) &&
      !test_dummy::RegisterNonMainDexNatives(env)) {
    return false;
  }
  if (!test_dummy::RegisterMainDexNatives(env)) {
    return false;
  }
  return true;
}

}  // extern "C"
