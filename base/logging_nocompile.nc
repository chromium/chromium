// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/logging.h"

// LOG(FATAL) must be understood as [[noreturn]].
int Foo() {
  LOG(FATAL) << "I am [[noreturn]]!";
  return 42;  // expected-error {{'return' will never be executed}}
}

// It's important that our logging macros agree on [[noreturn]] in all build
// configurations (or dead-code warnings become impossible to satisfy). As such
// neither LOG(DFATAL) or DLOG(FATAL) may be understood as [[noreturn]]. This
// non-void function not returning a value after LOG(DFATAL) and DLOG(FATAL)
// should always be a compile error due to a missing return statement.
int Bar() {
  LOG(DFATAL) << "I am not [[noreturn]]!";
  DLOG(FATAL) << "I am not [[noreturn]]!";
}  // expected-error {{non-void function does not return a value}}
