// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/threading/sequence_bound.h"

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

// `WithArgs()` may only be used with methods that accept arguments.
class NoArgs {
 public:
  void Method();
};

void CallWithArgsOnMethodTakingNoArgs() {
  SequenceBound<NoArgs> sq(SequencedTaskRunner::GetCurrentDefault());
  sq.AsyncCall(&NoArgs::Method).WithArgs(1);  // expected-error@*:* {{no member named 'WithArgs' in}}
}

// `Then()` may only be used after `WithArgs()`.
class HasArgs {
 public:
  void Method(int);
};

void CallThenBeforeWithArgs() {
  SequenceBound<HasArgs> sq(SequencedTaskRunner::GetCurrentDefault());
  sq.AsyncCall(&HasArgs::Method)
      .Then(DoNothing())  // expected-error@*:* {{no member named 'Then' in}}
      .WithArgs(1);
}

// TODO(crbug.com/40245687): Add no-compile tests for converting between
// SequenceBound<T> and SequenceBound<std::unique_ptr<T>>. This requires
// constraining the conversion first; see the TODO in sequence_bound.h.

}  // namespace base
