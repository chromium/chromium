// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/traits.h"

namespace base {

MemoryConsumerTraits::MemoryConsumerTraits(const MemoryConsumerTraits& other) =
    default;

MemoryConsumerTraits& MemoryConsumerTraits::operator=(
    const MemoryConsumerTraits& other) = default;

}  // namespace base
