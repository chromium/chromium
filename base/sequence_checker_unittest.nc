// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/sequence_checker.h"

namespace {

class SequenceAffine {
 public:
  void BuggyIncrement();

  void Increment() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    ++counter_;
  }

 private:
  int counter_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

#if defined(NCTEST_ACCESS_WITHOUT_CHECK)  // [r"fatal error: writing variable 'counter_' requires holding context 'sequence_checker_' exclusively"]

void SequenceAffine::BuggyIncrement() {
  // Member access without sequence_checker_ assertion.
  ++counter_;
}

#elif defined(NCTEST_CALL_WITHOUT_CHECK)  // [r"fatal error: calling function 'Increment' requires holding context 'sequence_checker_' exclusively"]

void SequenceAffine::BuggyIncrement() {
  // Function call without sequence_checker_ assertion.
  Increment();
}


#endif

}  // namespace
