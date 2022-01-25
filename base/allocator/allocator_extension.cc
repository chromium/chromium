// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension.h"
#include "base/allocator/buildflags.h"
#include "base/check.h"

namespace base {
namespace allocator {

void ReleaseFreeMemory() {}

// TODO(crbug.com/1257213): Remove the functions below, since they only existed
// for tcmalloc.
bool GetNumericProperty(const char* name, size_t* value) {
  return false;
}

bool SetNumericProperty(const char* name, size_t value) {
  return false;
}

void GetHeapSample(std::string* writer) {}

bool IsHeapProfilerRunning() {
  return false;
}

void SetHooks(AllocHookFunc alloc_hook, FreeHookFunc free_hook) {}

int GetCallStack(void** stack, int max_stack_size) {
  return 0;
}

}  // namespace allocator
}  // namespace base
