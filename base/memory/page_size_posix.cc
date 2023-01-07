// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/page_size.h"

#include <unistd.h>

namespace base {

size_t GetPageSize() {
  static const size_t pagesize = []() -> size_t {
  // For more information see getpagesize(2). Portable applications should use
  // sysconf(_SC_PAGESIZE) rather than getpagesize() if it's available.
#if defined(_SC_PAGESIZE)
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#else
    return getpagesize();
#endif
  }();
  return pagesize;
}

}  // namespace base
