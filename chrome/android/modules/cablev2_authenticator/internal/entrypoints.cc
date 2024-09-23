// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_utils.h"
#include "chrome/android/modules/cablev2_authenticator/internal/cablev2_authenticator__jni_registration_generated.h"
#include "third_party/jni_zero/jni_zero.h"

extern "C" {
// This JNI registration method is found and called by module framework code.
JNI_BOUNDARY_EXPORT bool JNI_OnLoad_cablev2_authenticator(JNIEnv* env) {
  if (!cablev2_authenticator::RegisterNatives(env)) {
    return false;
  }
  return true;
}

}  // extern "C"
