// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_command_line.h"

namespace base {
namespace test {

ScopedCommandLine::ScopedCommandLine()
    : original_command_line_(*base::CommandLine::ForCurrentProcess()) {}

ScopedCommandLine::~ScopedCommandLine() {
  *base::CommandLine::ForCurrentProcess() = original_command_line_;
}

CommandLine* ScopedCommandLine::GetProcessCommandLine() {
  return base::CommandLine::ForCurrentProcess();
}

}  // namespace test
}  // namespace base
