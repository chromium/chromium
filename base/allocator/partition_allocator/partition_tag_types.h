// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_TYPES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_TYPES_H_

#include <cstdint>

// This header defines the types for MTECheckedPtr. Canonical
// documentation available at `//base/memory/raw_ptr_mtecheckedptr.md`.

namespace partition_alloc {

// Use 8 bits for the partition tag. This is the "lower" byte of the
// two top bytes in a 64-bit pointer. The "upper" byte of the same
// is reserved for true ARM MTE.
//
// MTECheckedPtr is not yet compatible with ARM MTE, but it is a
// distant goal to have them coexist.
using PartitionTag = uint8_t;

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_TYPES_H_
