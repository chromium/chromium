// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_javatests_jni_headers/EarlyNativeTest_jni.h"
#include "base/command_line.h"
#include "base/trace_event/trace_log.h"
#include "base/tracing_buildflags.h"

namespace base {

// Ensures that the LibraryLoader swapped over to the native command line.
jboolean JNI_EarlyNativeTest_IsCommandLineInitialized(JNIEnv* env) {
  return CommandLine::InitializedForCurrentProcess();
}

// Ensures that native initialization took place, allowing early native code to
// use things like Tracing that don't depend on content initialization.
jboolean JNI_EarlyNativeTest_IsProcessNameEmpty(JNIEnv* env) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  return trace_event::TraceLog::GetInstance()->IsProcessNameEmpty();
#else   // BUILDFLAG(ENABLE_BASE_TRACING)
  return false;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

}  // namespace base
