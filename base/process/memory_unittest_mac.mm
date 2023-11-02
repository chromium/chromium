// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory_unittest_mac.h"

#include <CoreFoundation/CoreFoundation.h>

namespace base {

void* AllocateViaCFAllocatorSystemDefault(ssize_t size) {
  return CFAllocatorAllocate(kCFAllocatorSystemDefault, size, 0);
}

void* AllocateViaCFAllocatorMalloc(ssize_t size) {
  return CFAllocatorAllocate(kCFAllocatorMalloc, size, 0);
}

void* AllocateViaCFAllocatorMallocZone(ssize_t size) {
  return CFAllocatorAllocate(kCFAllocatorMallocZone, size, 0);
}

}  // namespace base
