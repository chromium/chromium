// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequenced_task_runner_handle.h"

namespace base {

// DEPRECATED: Use SequencedTaskRunner::GetCurrentDefault instead
// static
const scoped_refptr<SequencedTaskRunner>& SequencedTaskRunnerHandle::Get() {
  return SequencedTaskRunner::GetCurrentDefault();
}

// DEPRECATED: Use SequencedTaskRunner::HasCurrentDefault instead
// static
bool SequencedTaskRunnerHandle::IsSet() {
  return SequencedTaskRunner::HasCurrentDefault();
}

}  // namespace base
