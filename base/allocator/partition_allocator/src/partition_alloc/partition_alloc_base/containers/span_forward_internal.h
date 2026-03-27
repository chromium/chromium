// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_CONTAINERS_SPAN_FORWARD_INTERNAL_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_CONTAINERS_SPAN_FORWARD_INTERNAL_H_

#include <stddef.h>

#include <limits>

namespace partition_alloc::internal::base {

// [span.syn]: Constants
inline constexpr size_t dynamic_extent = std::numeric_limits<size_t>::max();

// [views.span]: class template `span<>`
template <typename ElementType,
          size_t Extent = dynamic_extent,
          // Storage pointer customization.
          typename InternalPtrType = ElementType*>
class span;

}  // namespace partition_alloc::internal::base

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_CONTAINERS_SPAN_FORWARD_INTERNAL_H_
