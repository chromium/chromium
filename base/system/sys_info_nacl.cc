// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <unistd.h>

namespace base {

// static
size_t SysInfo::VMAllocationGranularity() {
  return static_cast<size_t>(getpagesize());
}

}  // namespace base
