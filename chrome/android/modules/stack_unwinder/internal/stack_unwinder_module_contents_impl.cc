// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <type_traits>
#include <utility>

#include "chrome/android/features/stack_unwinder/public/function_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/modules/stack_unwinder/internal/jni_headers/StackUnwinderModuleContentsImpl_jni.h"

void DoNothing() {
  return;
}

static_assert(std::is_same<stack_unwinder::DoNothingFunction,
                           decltype(&DoNothing)>::value,
              "DoNothingFunction typedef must match the declared function "
              "type");

static jlong JNI_StackUnwinderModuleContentsImpl_GetDoNothingFunction(
    JNIEnv* env) {
  return reinterpret_cast<jlong>(&DoNothing);
}

DEFINE_JNI(StackUnwinderModuleContentsImpl)
