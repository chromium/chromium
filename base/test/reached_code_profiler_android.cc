// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/reached_code_profiler.h"
#include "base/test/test_support_jni_headers/ReachedCodeProfiler_jni.h"

// This file provides functions to query the state of the reached code profiler
// from Java. It's used only for tests.
namespace base {
namespace android {

static jboolean JNI_ReachedCodeProfiler_IsReachedCodeProfilerEnabled(
    JNIEnv* env) {
  return IsReachedCodeProfilerEnabled();
}

static jboolean JNI_ReachedCodeProfiler_IsReachedCodeProfilerSupported(
    JNIEnv* env) {
  return IsReachedCodeProfilerSupported();
}

}  // namespace android
}  // namespace base
