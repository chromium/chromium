// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/base_jni/ClangProfiler_jni.h"
#include "base/test/clang_profiling.h"

// Used in java tests when clang profiling is enabled.
namespace base {

static void JNI_ClangProfiler_WriteClangProfilingProfile(JNIEnv* env) {
  WriteClangProfilingProfile();
}

}  // namespace base
