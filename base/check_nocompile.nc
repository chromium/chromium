// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/check.h"
#include "base/not_fatal_until.h"

void OnlyNotFatalUntil() {
  CHECK(base::NotFatalUntil::M130);  // expected-error {{value of type 'base::NotFatalUntil' is not contextually convertible to 'bool'}}
}

void NotFatalUntilAsInt() {
  CHECK(true, 130);  // expected-error {{cannot initialize a parameter of type 'base::NotFatalUntil' with an rvalue of type 'int'}}
}
