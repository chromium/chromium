// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_log.h"
#include "base/tracing_buildflags.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_javatests_jni/EarlyNativeTest_jni.h"

namespace base {

// Ensures that the LibraryLoader swapped over to the native command line.
static jboolean JNI_EarlyNativeTest_IsCommandLineInitialized(JNIEnv* env) {
  return CommandLine::InitializedForCurrentProcess();
}

// Ensures that native initialization took place, allowing early native code to
// use things like Tracing that don't depend on content initialization.
static jboolean JNI_EarlyNativeTest_IsProcessNameEmpty(JNIEnv* env) {
  return CurrentProcess::GetInstance().IsProcessNameEmpty();
}

}  // namespace base

DEFINE_JNI(EarlyNativeTest)
