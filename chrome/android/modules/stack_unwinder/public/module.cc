// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/modules/stack_unwinder/public/module.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/modules/stack_unwinder/provider/jni_headers/StackUnwinderModuleProvider_jni.h"

namespace stack_unwinder {

// static
bool Module::IsInstalled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_StackUnwinderModuleProvider_isModuleInstalled(env);
}

// static
void Module::RequestInstallation() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_StackUnwinderModuleProvider_installModule(env);
}

// static
std::unique_ptr<Module> Module::Load() {
  CHECK(IsInstalled());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_StackUnwinderModuleProvider_ensureNativeLoaded(env);

  CreateMemoryRegionsMapFunction create_memory_regions_map =
      reinterpret_cast<CreateMemoryRegionsMapFunction>(
          Java_StackUnwinderModuleProvider_getCreateMemoryRegionsMapFunction(
              env));

  CreateNativeUnwinderFunction create_native_unwinder =
      reinterpret_cast<CreateNativeUnwinderFunction>(
          Java_StackUnwinderModuleProvider_getCreateNativeUnwinderFunction(
              env));

  CreateLibunwindstackUnwinderFunction create_libunwindstack_unwinder =
      reinterpret_cast<CreateLibunwindstackUnwinderFunction>(
          Java_StackUnwinderModuleProvider_getCreateLibunwindstackUnwinderFunction(
              env));

  return base::WrapUnique(new Module(create_memory_regions_map,
                                     create_native_unwinder,
                                     create_libunwindstack_unwinder));
}

std::unique_ptr<base::NativeUnwinderAndroidMemoryRegionsMap>
Module::CreateMemoryRegionsMap() {
  return create_memory_regions_map_();
}

std::unique_ptr<base::Unwinder> Module::CreateNativeUnwinder(
    base::NativeUnwinderAndroidMapDelegate* map_delegate,
    uintptr_t exclude_module_with_base_address) {
  return create_native_unwinder_(map_delegate,
                                 exclude_module_with_base_address);
}

std::unique_ptr<base::Unwinder> Module::CreateLibunwindstackUnwinder() {
  return create_libunwindstack_unwinder_();
}

Module::Module(
    CreateMemoryRegionsMapFunction create_memory_regions_map,
    CreateNativeUnwinderFunction create_native_unwinder,
    CreateLibunwindstackUnwinderFunction create_libunwindstack_unwinder)
    : create_memory_regions_map_(create_memory_regions_map),
      create_native_unwinder_(create_native_unwinder),
      create_libunwindstack_unwinder_(create_libunwindstack_unwinder) {
  DCHECK(create_memory_regions_map);
  DCHECK(create_native_unwinder);
  DCHECK(create_libunwindstack_unwinder);
}

}  // namespace stack_unwinder
