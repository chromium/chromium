// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/shim/allocator_dispatch.h"
#include "partition_alloc/shim/winheap_stubs_win.h"

// This file must not include any headers that include <malloc.h> or <new>.
// e.g. if <new> is included, we will see the following error:
// "redeclaration of 'operator new' should not add 'dllexport' attribute"

namespace std {
struct nothrow_t;
}  // namespace std

namespace allocator_shim {
namespace internal {

const allocator_shim::AllocatorDispatch* GetChainHead();
bool CallNewHandler(size_t size);
extern bool g_call_new_handler_on_malloc_failure;
size_t CheckedMultiply(size_t multiplicand, size_t multiplier);

}  // namespace internal

void SetCallNewHandlerOnMallocFailure(bool value);

}  // namespace allocator_shim

// No calls to malloc / new in this file. They would cause re-entrancy of
// the shim, which is hard to deal with. Keep this code as simple as possible
// and don't use any external C++ object here, not even //base ones. Even if
// they are safe to use today, in future they might be refactored.

#include "partition_alloc/shim/allocator_shim_override_ucrt_symbols_win.h"

// Cross-checks.
#if !defined(COMPONENT_BUILD) || !PA_BUILDFLAG(IS_WIN)
#error This code is only for Windows component build.
#endif

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#error The allocator shim should not be compiled when building for memory tools.
#endif

#if (defined(__GNUC__) && defined(__EXCEPTIONS)) || \
    (defined(_MSC_VER) && defined(_CPPUNWIND))
#error This code cannot be used when exceptions are turned on.
#endif
