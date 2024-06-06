// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim.h"
#include "build/build_config.h"
#include "build/rust/std/alias.h"
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
// Rustc as the final linker, we'll define those symbols here instead. This
// allows us to redirect allocation to PartitionAlloc if clang is doing the
// link.
//
// We use unchecked allocation paths in PartitionAlloc rather than going through
// its shims in `malloc()` etc so that we can support fallible allocation paths
// such as Vec::try_reserve without crashing on allocation failure.
//
// In future, we should build a crate with a #[global_allocator] and
// redirect these symbols back to Rust in order to use to that crate instead.
// This would allow Rust-linked executables to:
// 1. Use PartitionAlloc on Windows. The stdlib uses Windows heap functions
//    directly that PartitionAlloc can not intercept.
// 2. Have `Vec::try_reserve` to fail at runtime on Linux instead of crashing in
//    malloc() where PartitionAlloc replaces that function.
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

// This must exist as the stdlib depends on it to prove that we know the
// alloc shims below are unstable. In the future we may be required to replace
// them with a #[global_allocator] crate (see file comment above for more).
//
// Marked as weak as when Rust drives linking it includes this symbol itself,
// and we don't want a collision due to C++ being in the same link target, where
// C++ causes us to explicitly link in the stdlib and this symbol here.
[[maybe_unused]]
__attribute__((weak)) unsigned char __rust_no_alloc_shim_is_unstable;

REMAP_ALLOC_ATTRIBUTES void* __rust_alloc(size_t size, size_t align) {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  extern void* __rdl_alloc(size_t size, size_t align);
  return __rdl_alloc(size, align);
#else
  // PartitionAlloc will crash if given an alignment larger than this.
  if (align > partition_alloc::internal::kMaxSupportedAlignment) {
    return nullptr;
  }

  if (align <= alignof(std::max_align_t)) {
    return allocator_shim::UncheckedAlloc(size);
  } else {
    // TODO(b/342251590): We need an Unchecked path for aligned allocations.
    // Then we should use that instead of all these platform-specific functions
    // and enable the rest of the RustStaticTest.RustLargeAllocationFailure
    // test.

#if defined(COMPILER_MSVC)
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
#endif
}

REMAP_ALLOC_ATTRIBUTES void __rust_dealloc(void* p, size_t size, size_t align) {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  extern void __rdl_dealloc(void* p, size_t size, size_t align);
  return __rdl_dealloc(p, size, align);
#else
  if (align <= alignof(std::max_align_t)) {
    allocator_shim::UncheckedFree(p);
  } else {
#if defined(COMPILER_MSVC)
    _aligned_free(p);
#else
    free(p);
#endif
  }
#endif
}

REMAP_ALLOC_ATTRIBUTES void* __rust_realloc(void* p,
                                            size_t old_size,
                                            size_t align,
                                            size_t new_size) {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  extern void* __rdl_realloc(void* p, size_t old_size, size_t align,
                             size_t new_size);
  return __rdl_realloc(p, old_size, align, new_size);
#else
  // TODO(b/342251590): We need an Unchecked path for reallocations. Then we
  // should use that instead of `reallloc()` and enable the rest of the
  // RustStaticTest.RustLargeAllocationFailure test.

  if (align <= alignof(std::max_align_t)) {
    return realloc(p, new_size);
  } else {
    void* out = __rust_alloc(align, new_size);
    if (out) {
      memcpy(out, p, std::min(old_size, new_size));
    }
    return out;
  }
#endif
}

REMAP_ALLOC_ATTRIBUTES void* __rust_alloc_zeroed(size_t size, size_t align) {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  extern void* __rdl_alloc_zeroed(size_t size, size_t align);
  return __rdl_alloc_zeroed(size, align);
#else
  // TODO(danakj): It's possible that a partition_alloc::UncheckedAllocZeroed()
  // call would perform better than partition_alloc::UncheckedAlloc() + memset.
  // But there is no such API today. See b/342251590.
  void* p = __rust_alloc(size, align);
  if (p) {
    memset(p, 0, size);
  }
  return p;
#endif
}

REMAP_ALLOC_ATTRIBUTES void __rust_alloc_error_handler(size_t size,
                                                       size_t align) {
  NO_CODE_FOLDING();
  IMMEDIATE_CRASH();
}

REMAP_ALLOC_ATTRIBUTES extern const unsigned char
    __rust_alloc_error_handler_should_panic = 0;

}  // extern "C"
