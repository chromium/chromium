// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(thakis): libc++ removed the _LIBCPP_OVERRIDABLE_FUNC_VIS hook
// in https://github.com/llvm/llvm-project/pull/113139, and recommended
// fvisibility-global-new-delete=force-default as alternative. Sadly,
// this doesn't actually work, see the discussion on that PR. As a
// workaround, inject a header everywhere :/
// See crbug.com/379170490 for more details.

#ifndef BUILDTOOLS_THIRD_PARTY_LIBC___GROSS_HACK_H_
#define BUILDTOOLS_THIRD_PARTY_LIBC___GROSS_HACK_H_

#ifdef __cplusplus
#include <new>

// clang-format off
[[__nodiscard__]] __attribute__((__visibility__("default"))) void* operator new(std::size_t __sz, const std::nothrow_t&) _NOEXCEPT
    _LIBCPP_NOALIAS;
__attribute__((__visibility__("default"))) void operator delete(void* __p, const std::nothrow_t&) _NOEXCEPT;

[[__nodiscard__]] __attribute__((__visibility__("default"))) void* operator new[](std::size_t __sz, const std::nothrow_t&) _NOEXCEPT
    _LIBCPP_NOALIAS;
__attribute__((__visibility__("default"))) void operator delete[](void* __p, const std::nothrow_t&) _NOEXCEPT;

[[__nodiscard__]] __attribute__((__visibility__("default"))) void*
operator new(std::size_t __sz, std::align_val_t, const std::nothrow_t&) _NOEXCEPT _LIBCPP_NOALIAS;
__attribute__((__visibility__("default"))) void operator delete(void* __p, std::align_val_t, const std::nothrow_t&) _NOEXCEPT;

[[__nodiscard__]] __attribute__((__visibility__("default"))) void*
operator new[](std::size_t __sz, std::align_val_t, const std::nothrow_t&) _NOEXCEPT _LIBCPP_NOALIAS;
__attribute__((__visibility__("default"))) void operator delete[](void* __p, std::align_val_t, const std::nothrow_t&) _NOEXCEPT;
// clang-format on

#endif

#endif  // BUILDTOOLS_THIRD_PARTY_LIBC___GROSS_HACK_H_
