// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_H_
#define BASE_MEMORY_RAW_PTR_H_

#include "base/compiler_specific.h"

// TODO(crbug.com/40284755): raw_ptr should use UNSAFE_BUFFERS() internally.
UNSAFE_BUFFERS_INCLUDE_BEGIN
// Although `raw_ptr` is part of the standalone PA distribution, it is
// easier to use the shorter path in `//base/memory`. We retain this
// facade header for ease of typing.
#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr.h"  // IWYU pragma: export
UNSAFE_BUFFERS_INCLUDE_END

#endif  // BASE_MEMORY_RAW_PTR_H_
