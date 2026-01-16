// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_iterator.h"

#include "base/strings/string_util.h"

namespace base {

ProcessIterator::ProcessIterator(const ProcessFilter* filter)
    : snapshot_(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)),
      filter_(filter) {}

ProcessIterator::~ProcessIterator() {
  CloseHandle(snapshot_);
}

bool ProcessIterator::CheckForNextProcess() {
  entry_ = ProcessEntry{{.dwSize = sizeof(entry_)}};

  if (!started_iteration_) {
    started_iteration_ = true;
    return !!Process32First(snapshot_, &entry_);
  }

  return !!Process32Next(snapshot_, &entry_);
}

bool NamedProcessIterator::IncludeEntry() {
  FilePath::StringViewType entry_exe_view(entry().exe_file());

  return FilePath::CompareEqualIgnoreCase(executable_name_, entry_exe_view) &&
         ProcessIterator::IncludeEntry();
}

}  // namespace base
