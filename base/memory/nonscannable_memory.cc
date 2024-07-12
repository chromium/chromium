// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/nonscannable_memory.h"

#include <stdlib.h>

#include "partition_alloc/buildflags.h"

namespace base {

void* AllocNonScannable(size_t size) {
  return ::malloc(size);
}

void FreeNonScannable(void* ptr) {
  return ::free(ptr);
}

void* AllocNonQuarantinable(size_t size) {
  return ::malloc(size);
}

void FreeNonQuarantinable(void* ptr) {
  return ::free(ptr);
}

}  // namespace base
