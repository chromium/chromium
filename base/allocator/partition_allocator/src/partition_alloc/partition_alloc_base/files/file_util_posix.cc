// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/files/file_util.h"

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/containers/span.h"
#include "partition_alloc/partition_alloc_base/posix/eintr_wrapper.h"

namespace partition_alloc::internal::base {

bool ReadFromFD(int fd, span<char> buffer) {
  while (!buffer.empty()) {
    ssize_t bytes_read = WrapEINTR(read)(fd, buffer.data(), buffer.size());
    if (bytes_read <= 0) {
      break;
    }
    // "Peel" the portion that was successfully read from the front of the span.
    buffer = buffer.subspan(static_cast<size_t>(bytes_read));
  }
  return buffer.empty();
}

}  // namespace partition_alloc::internal::base
