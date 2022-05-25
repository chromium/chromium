// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MIGRATION_ADAPTER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MIGRATION_ADAPTER_H_

#include <cstdint>
#include <string>

#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace base {

class LapTimer;

template <typename Type, typename Traits>
class LazyInstance;

template <typename Type>
struct LazyInstanceTraitsBase;

#if BUILDFLAG(IS_ANDROID)
template <typename CharT, typename Traits>
class BasicStringPiece;
using StringPiece = BasicStringPiece<char, std::char_traits<char>>;
#endif

#if BUILDFLAG(IS_MAC)

namespace internal {

template <typename CFT>
struct ScopedCFTypeRefTraits;

}  // namespace internal

template <typename T, typename Traits>
class ScopedTypeRef;

namespace mac {

template <typename T>
T CFCast(const CFTypeRef& cf_val);
template <typename T>
T CFCastStrict(const CFTypeRef& cf_val);

bool IsAtLeastOS10_14();

}  // namespace mac

#endif  // BUILDFLAG(IS_MAC)

}  // namespace base

namespace partition_alloc::internal::base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::base::LapTimer;
using ::base::LazyInstance;
using ::base::LazyInstanceTraitsBase;

#if BUILDFLAG(IS_MAC)
template <typename CFT>
using ScopedCFTypeRef =
    ::base::ScopedTypeRef<CFT, ::base::internal::ScopedCFTypeRefTraits<CFT>>;
#endif

#if BUILDFLAG(IS_MAC)
namespace mac {

using ::base::mac::CFCast;
using ::base::mac::IsAtLeastOS10_14;

}  // namespace mac
#endif  // BUILDFLAG(IS_MAC)

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MIGRATION_ADAPTER_H_
