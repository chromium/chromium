// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/process/process_iterator.h"

namespace base {

ProcessIterator::ProcessIterator(const ProcessFilter* filter) {
  // TODO(crbug.com/1412835): Implement ProcessIterator on iOS.
  NOTREACHED();
}

ProcessIterator::~ProcessIterator() {}

bool ProcessIterator::CheckForNextProcess() {
  return false;
}

bool NamedProcessIterator::IncludeEntry() {
  return false;
}

}  // namespace base
