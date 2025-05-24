// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"
#include "base/time/time.h"

namespace base {

Process::Priority Process::GetPriority() const {
  return Priority::kUserBlocking;
}

bool Process::SetPriority(Priority priority) {
  return false;
}

Time Process::CreationTime() const {
  return Time();
}

}  // namespace base
