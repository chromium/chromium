// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstring>

#include "build/build_config.h"
#include "build/rust/std/alias.h"
#include "build/rust/std/immediate_crash.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/partition_alloc_constants.h"  // nogncheck
#include "partition_alloc/shim/allocator_shim.h"        // nogncheck
#elif BUILDFLAG(IS_WIN)
#include <cstdlib>
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
//
// # On Windows ASAN
//
// In ASAN builds, PartitionAlloc-Everywhere is disabled, meaning malloc() and
// friends in C++ do not go to PartitionAlloc. So we also don't point the Rust
// allocation functions at PartitionAlloc. Generally, this means we just direct
// them to the Standard Library's allocator.
//
// However, on Windows the Standard Library uses HeapAlloc() and Windows ASAN
// does *not* hook that method, so ASAN does not get to hear about allocations
// made in Rust. To resolve this, we redirect allocation to _aligned_malloc
// which Windows ASAN *does* hook.
//
// Note that there is a runtime option to make ASAN hook HeapAlloc() but
// enabling it breaks Win32 APIs like CreateProcess:
// https://issues.chromium.org/u/1/issues/368070343#comment29

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

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && BUILDFLAG(IS_WIN) && \
    defined(ADDRESS_SANITIZER)
#define USE_WIN_ALIGNED_MALLOC 1
#else
#define USE_WIN_ALIGNED_MALLOC 0
#endif

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
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // PartitionAlloc will crash if given an alignment larger than this.
  if (align > partition_alloc::internal::kMaxSupportedAlignment) {
    return nullptr;
  }

  if (align <= alignof(std::max_align_t)) {
    return allocator_shim::UncheckedAlloc(size);
  } else {
    return allocator_shim::UncheckedAlignedAlloc(size, align);
  }
#elif USE_WIN_ALIGNED_MALLOC
  return _aligned_malloc(size, align);
#else
  extern void* __rdl_alloc(size_t size, size_t align);
  return __rdl_alloc(size, align);
#endif
}

REMAP_ALLOC_ATTRIBUTES void __rust_dealloc(void* p, size_t size, size_t align) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (align <= alignof(std::max_align_t)) {
    allocator_shim::UncheckedFree(p);
  } else {
    allocator_shim::UncheckedAlignedFree(p);
  }
#elif USE_WIN_ALIGNED_MALLOC
  return _aligned_free(p);
#else
  extern void __rdl_dealloc(void* p, size_t size, size_t align);
  __rdl_dealloc(p, size, align);
#endif
}

REMAP_ALLOC_ATTRIBUTES void* __rust_realloc(void* p,
                                            size_t old_size,
                                            size_t align,
                                            size_t new_size) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (align <= alignof(std::max_align_t)) {
    return allocator_shim::UncheckedRealloc(p, new_size);
  } else {
    return allocator_shim::UncheckedAlignedRealloc(p, new_size, align);
  }
#elif USE_WIN_ALIGNED_MALLOC
  return _aligned_realloc(p, new_size, align);
#else
  extern void* __rdl_realloc(void* p, size_t old_size, size_t align,
                             size_t new_size);
  return __rdl_realloc(p, old_size, align, new_size);
#endif
}

REMAP_ALLOC_ATTRIBUTES void* __rust_alloc_zeroed(size_t size, size_t align) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) || USE_WIN_ALIGNED_MALLOC
  // TODO(danakj): When USE_PARTITION_ALLOC_AS_MALLOC is true, it's possible
  // that a partition_alloc::UncheckedAllocZeroed() call would perform better
  // than partition_alloc::UncheckedAlloc() + memset. But there is no such API
  // today. See b/342251590.
  void* p = __rust_alloc(size, align);
  if (p) {
    memset(p, 0, size);
  }
  return p;
#else
  extern void* __rdl_alloc_zeroed(size_t size, size_t align);
  return __rdl_alloc_zeroed(size, align);
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
