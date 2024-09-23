// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim.h"

#include <unistd.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

// No calls to malloc / new in this file. They would would cause re-entrancy of
// the shim, which is hard to deal with. Keep this code as simple as possible
// and don't use any external C++ object here, not even //base ones. Even if
// they are safe to use today, in future they might be refactored.

#include "partition_alloc/shim/allocator_shim_functions.h"
#include "partition_alloc/shim/shim_alloc_functions.h"

// Cpp symbols (new / delete) should always be routed through the shim layer
// except on Windows and macOS (except for PartitionAlloc-Everywhere) where the
// malloc intercept is deep enough that it also catches the cpp calls.
//
// In case of PartitionAlloc-Everywhere on macOS, malloc backed by
// allocator_shim::internal::PartitionMalloc crashes on OOM, and we need to
// avoid crashes in case of operator new() noexcept.  Thus, operator new()
// noexcept needs to be routed to
// allocator_shim::internal::PartitionMallocUnchecked through the shim layer.
#include "partition_alloc/shim/allocator_shim_override_cpp_symbols.h"

#include "partition_alloc/shim/allocator_shim_override_libc_symbols.h"

// Some glibc versions (until commit 6c444ad6e953dbdf9c7be065308a0a777)
// incorrectly call __libc_memalign() to allocate memory (see elf/dl-tls.c in
// glibc 2.23 for instance), and free() to free it. This causes issues for us,
// as we are then asked to free memory we didn't allocate.
//
// This only happened in glibc to allocate TLS storage metadata, and there are
// no other callers of __libc_memalign() there as of September 2020. To work
// around this issue, intercept this internal libc symbol to make sure that both
// the allocation and the free() are caught by the shim.
//
// This seems fragile, and is, but there is ample precedent for it, making it
// quite likely to keep working in the future. For instance, LLVM for LSAN uses
// this mechanism.

#if PA_BUILDFLAG(PA_LIBC_GLIBC) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_override_glibc_weak_symbols.h"
#endif

// Cross-checks.

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#error The allocator shim should not be compiled when building for memory tools.
#endif

#if (defined(__GNUC__) && defined(__EXCEPTIONS)) || \
    (defined(_MSC_VER) && defined(_CPPUNWIND))
#error This code cannot be used when exceptions are turned on.
#endif
