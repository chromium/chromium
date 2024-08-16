// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_iterator.h"

#include "base/notreached.h"

namespace base {

ProcessIterator::ProcessIterator(const ProcessFilter* filter) {
  // TODO(crbug.com/40721279): Implement ProcessIterator on Fuchsia.
  NOTREACHED();
}

ProcessIterator::~ProcessIterator() = default;

bool ProcessIterator::CheckForNextProcess() {
  return false;
}

bool NamedProcessIterator::IncludeEntry() {
  return false;
}

}  // namespace base
