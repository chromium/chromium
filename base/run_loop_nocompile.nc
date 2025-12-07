// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

void LvalueQuitClosure() {
  auto task_runner = SequencedTaskRunner::GetCurrentDefault();
  task_runner->PostTask(FROM_HERE, RunLoop().QuitClosure());         // expected-error@*:* {{'this' argument to member function 'QuitClosure' is an rvalue, but function has non-const lvalue ref-qualifier}}
  task_runner->PostTask(FROM_HERE, RunLoop().QuitWhenIdleClosure()); // expected-error@*:* {{'this' argument to member function 'QuitWhenIdleClosure' is an rvalue, but function has non-const lvalue ref-qualifier}}
}

}  // namespace base
