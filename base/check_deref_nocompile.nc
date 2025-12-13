// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for nocompile tests.
// It's not actually compiled, but is checked by the compiler for validity.

#include "base/check_deref.h"

#include <memory>

namespace {

void CheckDerefTemporary() {
  // CHECK_DEREF should not extend the lifetime of temporaries.
  // // expected-error@+1 {{temporary bound to local reference 'ref' will be destroyed at the end of the full-expression}}
  int& ref = CHECK_DEREF(std::make_unique<int>(123));
  (void)ref;
}

}  // namespace
