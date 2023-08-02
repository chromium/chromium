// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <malloc/malloc.h>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace base {

ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process) {}

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return WrapUnique(new ProcessMetrics(process));
}

int ProcessMetrics::GetOpenFdCount() const {
  // Provide a stub for now. -1 indicates an error.
  NOTIMPLEMENTED_LOG_ONCE();
  return -1;
}

}  // namespace base
