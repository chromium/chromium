// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_FILES_FILE_UTIL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_FILES_FILE_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "build/build_config.h"
#include "partition_alloc/partition_alloc_base/component_export.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace partition_alloc::internal::base {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// Read exactly |bytes| bytes from file descriptor |fd|, storing the result
// in |buffer|. This function is protected against EINTR and partial reads.
// Returns true iff |bytes| bytes have been successfully read from |fd|.
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
bool ReadFromFD(int fd, char* buffer, size_t bytes);

#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_FILES_FILE_UTIL_H_
