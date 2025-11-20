// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/partition_alloc_base/process/process_handle.h"

#include <windows.h>

namespace partition_alloc::internal::base {

ProcessId GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

}  // namespace partition_alloc::internal::base
