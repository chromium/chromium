// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_zero.h"

extern "C" {
// This JNI registration method is found and called by module framework
// code.
JNI_ZERO_BOUNDARY_EXPORT bool JNI_OnLoad_stack_unwinder(JNIEnv* env) {
  return true;
}
}  // extern "C"
