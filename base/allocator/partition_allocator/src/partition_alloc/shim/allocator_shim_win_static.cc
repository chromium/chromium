// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/numerics/checked_math.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/checked_multiply_win.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

// No calls to malloc / new in this file. They would cause re-entrancy of
// the shim, which is hard to deal with. Keep this code as simple as possible
// and don't use any external C++ object here, not even //base ones. Even if
// they are safe to use today, in future they might be refactored.

#include "partition_alloc/shim/allocator_shim_functions.h"
#include "partition_alloc/shim/allocator_shim_override_ucrt_symbols_win.h"

// Cross-checks.
#if defined(COMPONENT_BUILD) || !PA_BUILDFLAG(IS_WIN)
#error This code is only for Windows static build.
#endif

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#error The allocator shim should not be compiled when building for memory tools.
#endif

#if (defined(__GNUC__) && defined(__EXCEPTIONS)) || \
    (defined(_MSC_VER) && defined(_CPPUNWIND))
#error This code cannot be used when exceptions are turned on.
#endif
