// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/termination_target_setter.h"

#include "base/process/process.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "partition_alloc/page_allocator.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace performance_manager {

void TerminationTargetSetter::SetTerminationTarget(
    const ProcessNode* process_node) {
#if BUILDFLAG(IS_WIN)
  if (process_node == nullptr) {
    partition_alloc::SetProcessToTerminateOnCommitFailure(nullptr);
  } else {
    const base::Process& process = process_node->GetProcess();
    partition_alloc::SetProcessToTerminateOnCommitFailure(
        process.Duplicate().Release());
  }
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace performance_manager
