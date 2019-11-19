// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/module_cache.h"

namespace base {

// static
std::unique_ptr<ModuleCache::Module> ModuleCache::CreateModuleForAddress(
    uintptr_t address) {
  return nullptr;
}

}  // namespace base
