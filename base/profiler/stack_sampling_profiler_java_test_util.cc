// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler_java_test_util.h"

#include "base/location.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_profiler_test_support_jni/TestSupport_jni.h"

namespace base {

namespace {

struct UnwinderJavaTestSupportParams {
  OnceClosure closure;
  FunctionAddressRange range;
};

}  // namespace

void JNI_TestSupport_InvokeCallbackFunction(JNIEnv* env, jlong context) {
  const void* start_program_counter = GetProgramCounter();

  UnwinderJavaTestSupportParams* params =
      reinterpret_cast<UnwinderJavaTestSupportParams*>(context);
  if (!params->closure.is_null()) {
    std::move(params->closure).Run();
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();

  params->range = {start_program_counter, end_program_counter};
}

FunctionAddressRange callWithJavaFunction(OnceClosure closure) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  UnwinderJavaTestSupportParams params{std::move(closure), {}};
  base::Java_TestSupport_callWithJavaFunction(
      env, reinterpret_cast<uintptr_t>(&params));
  return params.range;
}

}  // namespace base
