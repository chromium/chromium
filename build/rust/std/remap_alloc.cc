// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "build/build_config.h"
#include "build/rust/std/immediate_crash.h"

#if BUILDFLAG(IS_ANDROID)
#include <malloc.h>
#endif

// When linking a final binary, rustc has to pick between either:
// * The default Rust allocator
// * Any #[global_allocator] defined in *any rlib in its dependency tree*
//   (https://doc.rust-lang.org/edition-guide/rust-2018/platform-and-target-support/global-allocators.html)
//
// In this latter case, this fact will be recorded in some of the metadata
// within the .rlib file. (An .rlib file is just a .a file, but does have
// additional metadata for use by rustc. This is, as far as I know, the only
// such metadata we would ideally care about.)
//
// In all the linked rlibs,
// * If 0 crates define a #[global_allocator], rustc uses its default allocator
// * If 1 crate defines a #[global_allocator], rustc uses that
// * If >1 crates define a #[global_allocator], rustc bombs out.
//
// Because rustc does these checks, it doesn't just have the __rust_alloc
// symbols defined anywhere (neither in the stdlib nor in any of these
// crates which have a #[global_allocator] defined.)
//
// Instead:
// Rust's final linking stage invokes dynamic LLVM codegen to create symbols
// for the basic heap allocation operations. It literally creates a
// __rust_alloc symbol at link time. Unless any crate has specified a
// #[global_allocator], it simply calls from __rust_alloc into
// __rdl_alloc, which is the default Rust allocator. The same applies to a
// few other symbols.
//
// We're not (always) using rustc for final linking. For cases where we're not
// Rustc as the final linker, we'll define those symbols here instead.
//
// The Rust stdlib on Windows uses GetProcessHeap() which will bypass
// PartitionAlloc, so we do not forward these functions back to the stdlib.
// Instead, we pass them to PartitionAlloc, while replicating functionality from
// the unix stdlib to allow them to provide their increased functionality on top
// of the system functions.
//
// In future, we may build a crate with a #[global_allocator] and
// redirect these symbols back to Rust in order to use to that crate instead.
//
// Instead of going through system functions like malloc() we may want to call
// into PA directly if we wished for Rust allocations to be in a different
// partition, or similar, in the future.
//
// They're weak symbols, because this file will sometimes end up in targets
// which are linked by rustc, and thus we would otherwise get duplicate
// definitions. The following definitions will therefore only end up being
// used in targets which are linked by our C++ toolchain.

extern "C" {

#ifdef COMPONENT_BUILD
#if BUILDFLAG(IS_WIN)
#define REMAP_ALLOC_ATTRIBUTES __declspec(dllexport) __attribute__((weak))
#else
#define REMAP_ALLOC_ATTRIBUTES \
  __attribute__((visibility("default"))) __attribute__((weak))
#endif
#else
#define REMAP_ALLOC_ATTRIBUTES __attribute__((weak))
#endif  // COMPONENT_BUILD

REMAP_ALLOC_ATTRIBUTES void* __rust_alloc(size_t size, size_t align) {
  // This mirrors kMaxSupportedAlignment from
  // base/allocator/partition_allocator/partition_alloc_constants.h.
  // ParitionAlloc will crash if given an alignment larger than this.
  constexpr size_t max_align = (1 << 21) / 2;
  if (align > max_align) {
    return nullptr;
  }

  if (align <= alignof(std::max_align_t)) {
    return malloc(size);
  } else {
    // Note: PartitionAlloc by default will route aligned allocations back to
    // malloc() (the fast path) if they are for a small enough alignment. So we
    // just unconditionally use aligned allocation functions here.
    // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.cc;l=219-226;drc=31d99ff4aa0cc0b75063325ff243e911516a5a6a

#if defined(COMPILER_MSVC)
    // Because we use PartitionAlloc() as the allocator, free() is able to find
    // this allocation, instead of the usual requirement to use _aligned_free().
    return _aligned_malloc(size, align);
#elif BUILDFLAG(IS_ANDROID)
    // Android has no posix_memalign() exposed:
    // https://source.chromium.org/chromium/chromium/src/+/main:base/memory/aligned_memory.cc;l=24-30;drc=e4622aaeccea84652488d1822c28c78b7115684f
    return memalign(align, size);
#else
    // The `align` from Rust is always a power of 2:
    // https://doc.rust-lang.org/std/alloc/struct.Layout.html#method.from_size_align.
    //
    // We get here only if align > alignof(max_align_t), which guarantees that
    // the alignment is both a power of 2 and even, which is required by
    // posix_memalign().
    //
    // The PartitionAlloc impl requires that the alignment is at least the same
    // as pointer-alignment. std::max_align_t is at least pointer-aligned as
    // well, so we satisfy that.
    void* p;
    auto ret = posix_memalign(&p, align, size);
    return ret == 0 ? p : nullptr;
#endif
  }
}

REMAP_ALLOC_ATTRIBUTES void __rust_dealloc(void* p, size_t size, size_t align) {
  free(p);
}

REMAP_ALLOC_ATTRIBUTES void* __rust_realloc(void* p,
                                            size_t old_size,
                                            size_t align,
                                            size_t new_size) {
  if (align <= alignof(std::max_align_t)) {
    return realloc(p, new_size);
  } else {
    void* out = __rust_alloc(align, new_size);
    memcpy(out, p, std::min(old_size, new_size));
    return out;
  }
}

REMAP_ALLOC_ATTRIBUTES void* __rust_alloc_zeroed(size_t size, size_t align) {
  if (align <= alignof(std::max_align_t)) {
    return calloc(size, 1);
  } else {
    void* p = __rust_alloc(size, align);
    memset(p, 0, size);
    return p;
  }
}

REMAP_ALLOC_ATTRIBUTES void __rust_alloc_error_handler(size_t size,
                                                       size_t align) {
  IMMEDIATE_CRASH();
}

REMAP_ALLOC_ATTRIBUTES extern const unsigned char
    __rust_alloc_error_handler_should_panic = 0;

}  // extern "C"
