// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/strings/lazy_string_builder.h"

#include <string>

#include "base/strings/string_number_conversions.h"

// Verify that passing an rvalue string to LazyStringBuilder::AppendByReference() will cause
// a compile error.

namespace base {
namespace {

void WontCompileRValueString() {
  auto builder = LazyStringBuilder::CreateForTesting();
  std::string movable = "movable";
  builder.AppendByReference(std::string("foo"));  // expected-error {{call to deleted member function}}
  builder.AppendByReference(NumberToString(3));  // expected-error {{call to deleted member function}}
  builder.AppendByReference(std::move(movable));  // expected-error {{call to deleted member function}}
  builder.AppendByReference("hello ", std::string("world"));  // expected-error@base/strings/lazy_string_builder.h:* {{static assertion failed}}
  builder.AppendByReference("value: ", NumberToString(7));  // expected-error@base/strings/lazy_string_builder.h:* {{static assertion failed}}
  builder.AppendByReference("", std::move(movable));  // expected-error@base/strings/lazy_string_builder.h:* {{static assertion failed}}
}

void WontCompileConstructorAccessRestricted() {
  LazyStringBuilder builder1;  // expected-error {{no matching constructor}}
  LazyStringBuilder builder2({});  // expected-error {{calling a private constructor}}
  LazyStringBuilder builder3{LazyStringBuilder::AccessKey()};  // expected-error {{calling a private constructor}}
}

}  // namespace
}  // namespace base
