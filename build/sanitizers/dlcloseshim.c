// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// In due course we may need to replicate more of the complexity from
//  base/allocator/partition_allocator/src/partition_alloc/
//  shim/allocator_shim_internals.h
// but as we're targeting just libfuzzer Linux builds, perhaps we don't need
// it.

#if defined(__clang__)
__attribute__((visibility("default"), noinline))
#endif
void __wrap_dlclose(void *handle) {
  // Do nothing. We don't want to call the real dlclose on libfuzzer builds.
}
