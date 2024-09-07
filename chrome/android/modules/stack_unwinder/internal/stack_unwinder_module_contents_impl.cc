// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <type_traits>
#include <utility>

#include "base/profiler/libunwindstack_unwinder_android.h"
#include "base/profiler/native_unwinder_android.h"
#include "base/profiler/native_unwinder_android_memory_regions_map.h"
#include "chrome/android/features/stack_unwinder/public/function_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/modules/stack_unwinder/internal/jni_headers/StackUnwinderModuleContentsImpl_jni.h"

std::unique_ptr<base::NativeUnwinderAndroidMemoryRegionsMap>
CreateMemoryRegionsMap() {
  return base::NativeUnwinderAndroid::CreateMemoryRegionsMap();
}

static_assert(std::is_same<stack_unwinder::CreateMemoryRegionsMapFunction,
                           decltype(&CreateMemoryRegionsMap)>::value,
              "CreateMemoryRegionsMapFunction typedef must match the declared "
              "function type");

std::unique_ptr<base::Unwinder> CreateNativeUnwinder(
    base::NativeUnwinderAndroidMapDelegate* map_delegate,
    uintptr_t exclude_module_with_base_address) {
  return std::make_unique<base::NativeUnwinderAndroid>(
      exclude_module_with_base_address, map_delegate);
}

static_assert(std::is_same<stack_unwinder::CreateNativeUnwinderFunction,
                           decltype(&CreateNativeUnwinder)>::value,
              "CreateNativeUnwinderFunction typedef must match the declared "
              "function type");

std::unique_ptr<base::Unwinder> CreateLibunwindstackUnwinder() {
  return std::make_unique<base::LibunwindstackUnwinderAndroid>();
}

static_assert(
    std::is_same<stack_unwinder::CreateLibunwindstackUnwinderFunction,
                 decltype(&CreateLibunwindstackUnwinder)>::value,
    "CreateLibunwindstackUnwinderFunction typedef must match the declared "
    "function type");

static jlong
JNI_StackUnwinderModuleContentsImpl_GetCreateMemoryRegionsMapFunction(
    JNIEnv* env) {
  return reinterpret_cast<jlong>(&CreateMemoryRegionsMap);
}

static jlong
JNI_StackUnwinderModuleContentsImpl_GetCreateNativeUnwinderFunction(
    JNIEnv* env) {
  return reinterpret_cast<jlong>(&CreateNativeUnwinder);
}

static jlong
JNI_StackUnwinderModuleContentsImpl_GetCreateLibunwindstackUnwinderFunction(
    JNIEnv* env) {
  return reinterpret_cast<jlong>(&CreateLibunwindstackUnwinder);
}
