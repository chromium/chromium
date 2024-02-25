// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/files/file_util.h"

#include "partition_alloc/partition_alloc_base/posix/eintr_wrapper.h"

namespace partition_alloc::internal::base {

bool ReadFromFD(int fd, char* buffer, size_t bytes) {
  size_t total_read = 0;
  while (total_read < bytes) {
    ssize_t bytes_read =
        WrapEINTR(read)(fd, buffer + total_read, bytes - total_read);
    if (bytes_read <= 0) {
      break;
    }
    total_read += bytes_read;
  }
  return total_read == bytes;
}

}  // namespace partition_alloc::internal::base
