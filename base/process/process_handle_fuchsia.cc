// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <lib/zx/process.h>
#include <zircon/process.h>

#include "base/fuchsia/koid.h"
#include "base/logging.h"

namespace base {

ProcessId GetCurrentProcId() {
  return GetProcId(GetCurrentProcessHandle());
}

ProcessHandle GetCurrentProcessHandle() {
  // Note that zx_process_self() returns a real handle, and ownership is not
  // transferred to the caller (i.e. this should never be closed).
  return zx_process_self();
}

ProcessId GetProcId(ProcessHandle process) {
  return GetKoid(*zx::unowned_process(process)).value_or(ZX_KOID_INVALID);
}

}  // namespace base
