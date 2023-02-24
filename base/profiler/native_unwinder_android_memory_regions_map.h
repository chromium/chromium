// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_NATIVE_UNWINDER_ANDROID_MEMORY_REGIONS_MAP_H_
#define BASE_PROFILER_NATIVE_UNWINDER_ANDROID_MEMORY_REGIONS_MAP_H_

#include <memory>

namespace unwindstack {
class Maps;
class Memory;
}  // namespace unwindstack

namespace base {

// NativeUnwinderAndroidMemoryRegionsMap is an opaque interface that hides
// concrete libunwindstack types, i.e. `unwindstack::Maps` and
// `unwindstack::Memory`. By introducing the interface, chrome code can
// pass the underlying instances around without referencing libunwindstack.
// NativeUnwinderAndroidMemoryRegionsMap's implementation must live in the
// stack_unwinder dynamic feature module.
class NativeUnwinderAndroidMemoryRegionsMap {
 public:
  NativeUnwinderAndroidMemoryRegionsMap() = default;
  virtual ~NativeUnwinderAndroidMemoryRegionsMap() = default;

  NativeUnwinderAndroidMemoryRegionsMap(
      const NativeUnwinderAndroidMemoryRegionsMap&) = delete;
  NativeUnwinderAndroidMemoryRegionsMap& operator=(
      const NativeUnwinderAndroidMemoryRegionsMap&) = delete;

  virtual unwindstack::Maps* GetMaps() = 0;
  virtual unwindstack::Memory* GetMemory() = 0;
  // This function exists to provide a method for
  // `LibunwindstackUnwinderAndroid` to take the ownership of `Memory`.
  virtual std::unique_ptr<unwindstack::Memory> TakeMemory() = 0;
};

}  // namespace base

#endif  // BASE_PROFILER_NATIVE_UNWINDER_ANDROID_MEMORY_REGIONS_MAP_H_
