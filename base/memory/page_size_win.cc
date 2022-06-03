// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/page_size.h"

namespace base {

size_t GetPageSize() {
  // System pagesize. This value remains constant on x86/64 architectures.
  constexpr int PAGESIZE_KB = 4;
  return PAGESIZE_KB * 1024;
}

}  // namespace base
