// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_
#define CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_

#include <memory>

#include "base/profiler/native_unwinder_android_memory_regions_map.h"
#include "base/profiler/unwinder.h"
#include "chrome/android/features/stack_unwinder/public/function_types.h"

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

  // Stub function
  // TODO(crbug.com/398885436): Remove this function and clean up the support
  // machinery for executing native code.
  void DoNothing();

 private:
  explicit Module(DoNothingFunction do_nothing);

  const DoNothingFunction do_nothing_;
};

}  // namespace stack_unwinder

#endif  // CHROME_ANDROID_MODULES_STACK_UNWINDER_PUBLIC_MODULE_H_
