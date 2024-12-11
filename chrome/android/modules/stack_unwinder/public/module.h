// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_
#define CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_

#include <memory>

#include "base/profiler/native_unwinder_android_memory_regions_map.h"
#include "base/profiler/unwinder.h"
#include "chrome/android/features/stack_unwinder/public/function_types.h"

namespace base {
class NativeUnwinderAndroidMapDelegate;
}  // namespace base

namespace stack_unwinder {

// Provides access to the stack_unwinder module.
class Module {
 public:
  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;

  // Returns true if the module is installed.
  static bool IsInstalled();

  // Requests asynchronous installation of the module. This is a no-op if the
  // module is already installed.
  static void RequestInstallation();

  // Attempts to load the module. May be invoked only if IsInstalled().
  static std::unique_ptr<Module> Load();

  // Returns a map representing the current memory regions (modules, stacks,
  // etc.).
  std::unique_ptr<base::NativeUnwinderAndroidMemoryRegionsMap>
  CreateMemoryRegionsMap();

  // Creates a new native stack unwinder.
  std::unique_ptr<base::Unwinder> CreateNativeUnwinder(
      base::NativeUnwinderAndroidMapDelegate* map_delegate,
      uintptr_t exclude_module_with_base_address);

  // Creates an unwinder that will use libunwindstack::Unwinder exclusively, it
  // does not do partial unwinds instead either succeeding or failing the whole
  // stack. Should generally be used by itself rather then as part of a list of
  // base::Unwinders. Internally it manages its own MemoryRegionsMap and thus
  // doesn't take them in the constructor.
  std::unique_ptr<base::Unwinder> CreateLibunwindstackUnwinder();

 private:
  Module(CreateMemoryRegionsMapFunction create_memory_regions_map,
         CreateNativeUnwinderFunction create_native_unwinder,
         CreateLibunwindstackUnwinderFunction create_libunwindstack_unwinder);

  const CreateMemoryRegionsMapFunction create_memory_regions_map_;
  const CreateNativeUnwinderFunction create_native_unwinder_;
  const CreateLibunwindstackUnwinderFunction create_libunwindstack_unwinder_;
};

}  // namespace stack_unwinder

#endif  // CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_
