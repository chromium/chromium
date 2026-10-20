// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Weak fallbacks of the operator new variants; see alloc_token_aliases.c.
// They are in a separate target so that binaries without the C++ library don't
// reference operator new.

#include <stddef.h>

#include <new>

#if defined(_WIN32)
#define ALLOC_TOKEN_VISIBILITY
#elif defined(__APPLE__)
#define ALLOC_TOKEN_VISIBILITY __attribute__((visibility("hidden")))
#else
#define ALLOC_TOKEN_VISIBILITY __attribute__((visibility("default")))
#endif

#define ALLOC_TOKEN_FALLBACK           \
  __attribute__((weak, used, noinline, \
                 no_sanitize("alloc-token"))) ALLOC_TOKEN_VISIBILITY

extern "C" {

#if defined(_WIN32)

// With the MSVC ABI, only the following variants are rewritten. Their names
// are not valid identifiers, hence __identifier.
#if defined(_M_IX86)
#define ALLOC_TOKEN_NEW __identifier("__alloc_token_??2@YAPAXI@Z")
#define ALLOC_TOKEN_NEW_ARRAY __identifier("__alloc_token_??_U@YAPAXI@Z")
#define ALLOC_TOKEN_NEW_NOTHROW \
  __identifier("__alloc_token_??2@YAPAXIABUnothrow_t@std@@@Z")
#define ALLOC_TOKEN_NEW_ARRAY_NOTHROW \
  __identifier("__alloc_token_??_U@YAPAXIABUnothrow_t@std@@@Z")
#else
#define ALLOC_TOKEN_NEW __identifier("__alloc_token_??2@YAPEAX_K@Z")
#define ALLOC_TOKEN_NEW_ARRAY __identifier("__alloc_token_??_U@YAPEAX_K@Z")
#define ALLOC_TOKEN_NEW_NOTHROW \
  __identifier("__alloc_token_??2@YAPEAX_KAEBUnothrow_t@std@@@Z")
#define ALLOC_TOKEN_NEW_ARRAY_NOTHROW \
  __identifier("__alloc_token_??_U@YAPEAX_KAEBUnothrow_t@std@@@Z")
#endif

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(size_t size, size_t) {
  return ::operator new(size);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW_ARRAY(size_t size, size_t) {
  return ::operator new[](size);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW_NOTHROW(size_t size,
                                                   const std::nothrow_t&,
                                                   size_t) noexcept {
  return ::operator new(size, std::nothrow);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW_ARRAY_NOTHROW(size_t size,
                                                         const std::nothrow_t&,
                                                         size_t) noexcept {
  return ::operator new[](size, std::nothrow);
}

#undef ALLOC_TOKEN_NEW
#undef ALLOC_TOKEN_NEW_ARRAY
#undef ALLOC_TOKEN_NEW_NOTHROW
#undef ALLOC_TOKEN_NEW_ARRAY_NOTHROW

#else  // defined(_WIN32)

// size_t is mangled as m (unsigned long) or j (unsigned int).
#if __SIZEOF_SIZE_T__ == __SIZEOF_INT__
#define ALLOC_TOKEN_SIZE_T_MANGLING j
#else
#define ALLOC_TOKEN_SIZE_T_MANGLING m
#endif
#define ALLOC_TOKEN_CONCAT_IMPL(prefix, op, size_t_mangling, suffix) \
  prefix##op##size_t_mangling##suffix
#define ALLOC_TOKEN_CONCAT(prefix, op, size_t_mangling, suffix) \
  ALLOC_TOKEN_CONCAT_IMPL(prefix, op, size_t_mangling, suffix)
// e.g. ALLOC_TOKEN_NEW(w, RKSt9nothrow_t) is __alloc_token__ZnwmRKSt9nothrow_t
// on 64-bit platforms.
#define ALLOC_TOKEN_NEW(op, suffix) \
  ALLOC_TOKEN_CONCAT(__alloc_token__Zn, op, ALLOC_TOKEN_SIZE_T_MANGLING, suffix)

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(w, )(size_t size, size_t token) {
  return ::operator new(size);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(a, )(size_t size, size_t token) {
  return ::operator new[](size);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(w, RKSt9nothrow_t)(
    size_t size,
    const std::nothrow_t& nothrow,
    size_t token) noexcept {
  return ::operator new(size, std::nothrow);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(a, RKSt9nothrow_t)(
    size_t size,
    const std::nothrow_t& nothrow,
    size_t token) noexcept {
  return ::operator new[](size, std::nothrow);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(
    w,
    St11align_val_t)(size_t size, std::align_val_t alignment, size_t token) {
  return ::operator new(size, alignment);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(
    a,
    St11align_val_t)(size_t size, std::align_val_t alignment, size_t token) {
  return ::operator new[](size, alignment);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(w, St11align_val_tRKSt9nothrow_t)(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t& nothrow,
    size_t token) noexcept {
  return ::operator new(size, alignment, std::nothrow);
}

ALLOC_TOKEN_FALLBACK void* ALLOC_TOKEN_NEW(a, St11align_val_tRKSt9nothrow_t)(
    size_t size,
    std::align_val_t alignment,
    const std::nothrow_t& nothrow,
    size_t token) noexcept {
  return ::operator new[](size, alignment, std::nothrow);
}

#endif  // defined(_WIN32)

}  // extern "C"
