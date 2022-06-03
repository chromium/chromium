// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_FEATURES_STACK_UNWINDER_PUBLIC_MEMORY_REGIONS_MAP_H_
#define CHROME_ANDROID_FEATURES_STACK_UNWINDER_PUBLIC_MEMORY_REGIONS_MAP_H_

namespace stack_unwinder {

// MemoryRegionsMap is intended to provide an opaque interface to Chrome code.
// It must only be subclassed within the module implementation.
class MemoryRegionsMap {
 public:
  MemoryRegionsMap();
  virtual ~MemoryRegionsMap() = 0;

  MemoryRegionsMap(const MemoryRegionsMap&) = delete;
  MemoryRegionsMap& operator=(const MemoryRegionsMap&) = delete;
};

}  // namespace stack_unwinder

#endif  // CHROME_ANDROID_FEATURES_STACK_UNWINDER_PUBLIC_MEMORY_REGIONS_MAP_H_
