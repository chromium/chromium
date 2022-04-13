// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_BASE_MIGRATION_ADAPTER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_BASE_MIGRATION_ADAPTER_H_

#include <cstdint>
#include <string>

#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace base {

class LapTimer;
class PlatformThread;
class PlatformThreadHandle;
class PlatformThreadRef;
class TimeDelta;
class TimeTicks;
class CPU;

template <typename Type, typename Traits>
class LazyInstance;

template <typename Type>
struct LazyInstanceTraitsBase;

template <typename T>
constexpr TimeDelta Seconds(T n);
template <typename T>
constexpr TimeDelta Milliseconds(T n);
template <typename T>
constexpr TimeDelta Microseconds(T n);

BASE_EXPORT uint64_t RandGenerator(uint64_t range);
BASE_EXPORT std::string StringPrintf(const char* format, ...);

template <typename T, typename O>
class NoDestructor;

namespace debug {

void BASE_EXPORT Alias(const void* var);

}  // namespace debug

namespace internal {

template <typename T>
class CheckedNumeric;

}

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
bool IsOS10_11();

}  // namespace mac
#endif  // BUILDFLAG(IS_MAC)

}  // namespace base

namespace partition_alloc::internal::base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::base::CPU;
using ::base::LapTimer;
using ::base::LazyInstance;
using ::base::LazyInstanceTraitsBase;
using ::base::Microseconds;
using ::base::Milliseconds;
using ::base::NoDestructor;
using ::base::PlatformThread;
using ::base::PlatformThreadHandle;
using ::base::PlatformThreadRef;
using ::base::RandGenerator;
using ::base::Seconds;
using ::base::StringPrintf;
using ::base::TimeDelta;
using ::base::TimeTicks;
using ::base::internal::CheckedNumeric;

#if BUILDFLAG(IS_MAC)
template <typename CFT>
using ScopedCFTypeRef =
    ::base::ScopedTypeRef<CFT, ::base::internal::ScopedCFTypeRefTraits<CFT>>;
#endif

namespace debug {

using ::base::debug::Alias;

}  // namespace debug

#if BUILDFLAG(IS_MAC)
namespace mac {

using ::base::mac::CFCast;
using ::base::mac::IsAtLeastOS10_14;
using ::base::mac::IsOS10_11;

}  // namespace mac
#endif  // BUILDFLAG(IS_MAC)

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_BASE_MIGRATION_ADAPTER_H_
