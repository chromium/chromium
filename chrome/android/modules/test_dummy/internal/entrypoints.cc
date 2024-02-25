// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_utils.h"
#include "chrome/android/modules/test_dummy/internal/test_dummy__jni_registration_generated.h"
#include "third_party/jni_zero/jni_zero.h"

extern "C" {
// This JNI registration method is found and called by module framework code.
JNI_BOUNDARY_EXPORT bool JNI_OnLoad_test_dummy(JNIEnv* env) {
  if (!test_dummy::RegisterNatives(env)) {
    return false;
  }
  return true;
}

}  // extern "C"
