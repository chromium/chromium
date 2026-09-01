// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_FILES_FILE_UTIL_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_FILES_FILE_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/containers/span.h"

#if PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace partition_alloc::internal::base {

#if PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)

// Reads exactly as many bytes as `buffer` can hold from file descriptor `fd`
// into `buffer`. This function is protected against EINTR and partial reads.
// Returns true iff `buffer` was successfully filled with bytes read from `fd`.
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
bool ReadFromFD(int fd, span<char> buffer);

#endif  // PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)

}  // namespace partition_alloc::internal::base

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_FILES_FILE_UTIL_H_
