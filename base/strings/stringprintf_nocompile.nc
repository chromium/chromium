// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/strings/stringprintf.h"

#include <tuple>

#include "base/strings/cstring_view.h"

namespace base {

void ConstexprStringView() {
  static constexpr base::cstring_view kTest = "test %s";
  std::ignore = StringPrintfNonConstexpr(kTest.data(), "123");  // expected-error {{call to deleted function 'StringPrintfNonConstexpr'}}
}

void ConstexprCharArray() {
  static constexpr char kTest[] = "test %s";
  std::ignore = StringPrintfNonConstexpr(kTest, "123");  // expected-error {{call to deleted function 'StringPrintfNonConstexpr'}}
}

void ConstexprCharPointer() {
  static constexpr const char* kTest = "test %s";
  std::ignore = StringPrintfNonConstexpr(kTest, "123");  // expected-error {{call to deleted function 'StringPrintfNonConstexpr'}}
}

}  // namespace base
