// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_
#define CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_

#include <memory>

#include "base/profiler/unwinder.h"
#include "chrome/android/features/stack_unwinder/public/function_types.h"
#include "chrome/android/features/stack_unwinder/public/memory_regions_map.h"

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
  std::unique_ptr<MemoryRegionsMap> CreateMemoryRegionsMap();

  // Creates a new native stack unwinder.
  std::unique_ptr<base::Unwinder> CreateNativeUnwinder(
      MemoryRegionsMap* memory_regions_map,
      uintptr_t exclude_module_with_base_address);

 private:
  Module(CreateMemoryRegionsMapFunction create_memory_regions_map,
         CreateNativeUnwinderFunction create_native_unwinder);

  const CreateMemoryRegionsMapFunction create_memory_regions_map_;
  const CreateNativeUnwinderFunction create_native_unwinder_;
};

}  // namespace stack_unwinder

#endif  // CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_
