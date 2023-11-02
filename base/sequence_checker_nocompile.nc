// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/sequence_checker.h"

namespace {

class SequenceAffine {
 public:
  void BuggyCounterAccess();
  void BuggyIncrementCall();

  void Increment() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    ++counter_;
  }

 private:
  int counter_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

#if DCHECK_IS_ON()

void SequenceAffine::BuggyCounterAccess() {
  // Member access without sequence_checker_ assertion.
  ++counter_;  // expected-error {{writing variable 'counter_' requires holding context 'sequence_checker_' exclusively}}
}

void SequenceAffine::BuggyIncrementCall() {
  // Function call without sequence_checker_ assertion.
  Increment();  // expected-error {{calling function 'Increment' requires holding context 'sequence_checker_' exclusively}}
}

#else

// The SEQUENCE_CHECKER macros only do something in DCHECK builds.
// expected-no-diagnostics

#endif

}  // namespace
