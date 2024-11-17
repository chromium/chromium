// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_cookie.h"

#include <cstdint>
#include <type_traits>

#include "partition_alloc/partition_alloc_check.h"

#if PA_BUILDFLAG(USE_PARTITION_COOKIE)
namespace partition_alloc::internal {
[[noreturn]] PA_NOINLINE PA_NOT_TAIL_CALLED void CookieCorruptionDetected(
    unsigned char* cookie_ptr,
    size_t slot_usable_size) {
  using CookieValue = std::conditional_t<kCookieSize == 4, uint32_t, uint64_t>;
  static_assert(sizeof(CookieValue) <= kCookieSize);
  CookieValue cookie =
      *static_cast<CookieValue*>(static_cast<void*>(cookie_ptr));
  PA_DEBUG_DATA_ON_STACK("slotsize", slot_usable_size);
  PA_DEBUG_DATA_ON_STACK("cookie", cookie);

  PA_NO_CODE_FOLDING();
  PA_IMMEDIATE_CRASH();
}
}  // namespace partition_alloc::internal
#endif  // PA_BUILDFLAG(USE_PARTITION_COOKIE)
