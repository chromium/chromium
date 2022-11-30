// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/strings/string_piece.h"

#include <string>

namespace base {

#if defined(NCTEST_DANGLING_XVALUE)  // [r"object backing the pointer will be destroyed at the end of the full-expression"]

// Returns a std::string xvalue (temporary object).
std::string f() { return std::string(); };


void HoldsDanglingReferenceToString() {
  [[maybe_unused]] StringPiece piece = f();
}

#endif

}  // namespace base
