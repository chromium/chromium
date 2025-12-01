// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/memory/ref_counted.h"

#include <limits>
#include <ostream>
#include <type_traits>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"

namespace partition_alloc::internal::base::subtle {

bool RefCountedThreadSafeBase::HasOneRef() const {
  return ref_count_.IsOne();
}

bool RefCountedThreadSafeBase::HasAtLeastOneRef() const {
  return !ref_count_.IsZero();
}

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
RefCountedThreadSafeBase::~RefCountedThreadSafeBase() {
  PA_BASE_DCHECK(in_dtor_) << "RefCountedThreadSafe object deleted without "
                              "calling Release()";
}
#endif

// For security and correctness, we check the arithmetic on ref counts.
//
// In an attempt to avoid binary bloat (from inlining the `CHECK`), we define
// these functions out-of-line. However, compilers are wily. Further testing may
// show that `PA_NOINLINE` helps or hurts.
//
#if !PA_BUILDFLAG(PA_ARCH_CPU_X86_FAMILY)
bool RefCountedThreadSafeBase::Release() const {
  return ReleaseImpl();
}
void RefCountedThreadSafeBase::AddRef() const {
  AddRefImpl();
}
void RefCountedThreadSafeBase::AddRefWithCheck() const {
  AddRefWithCheckImpl();
}
#endif

}  // namespace partition_alloc::internal::base::subtle
