// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_limit.h"

#include "base/byte_size.h"

namespace base {

ByteSize MemoryLimit::Scale(ByteSize baseline) const {
  // Use int64_t here in order to get saturating behaviour if we get too big.
  const int64_t tmp = static_cast<int64_t>(baseline.InBytes());
  return ByteSize(static_cast<uint64_t>(Scale(tmp)));
}

}  // namespace base
