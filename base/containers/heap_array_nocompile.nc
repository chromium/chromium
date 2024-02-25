// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/heap_array.h"

#include <memory>

namespace base {
namespace {

struct ConstructorRequiresArgs {
  ConstructorRequiresArgs(int val) : val_(val) {}
  int val_;
};

struct NonTrivialClass {
  std::unique_ptr<int> ptr_;
};

void WontCompileUninithNonTrivialClass() {
  auto vec = HeapArray<NonTrivialClass>::Uninit(2u);  // expected-error {{constraints not satisfied}}
}

void WontCompileWithSizeConstructorRequiresArgs() {
  auto vec = HeapArray<ConstructorRequiresArgs>::WithSize(2u);  // expected-error {{constraints not satisfied}}
}

void WontCompileUninitConstructorRequiresArgs() {
  auto vec = base::HeapArray<ConstructorRequiresArgs>::Uninit(2u);  // expected-error {{constraints not satisfied}}
}

void WontCompileConstNotAllowed() {
  auto vec = base::HeapArray<const int>();  // expected-error@*:* {{HeapArray cannot hold const types}}
}

void WontCompileReferencesNotAllowed() {
  auto vec = base::HeapArray<int&>();  // expected-error@*:* {{HeapArray cannot hold reference types}}
}

int* WontCompileDataLifetime() {
  return HeapArray<int>::WithSize(1u).data();  // expected-error {{returning address}}
}

HeapArray<int>::iterator WontCompileBeginLifetime() {
  return HeapArray<int>::WithSize(1u).begin();  // expected-error {{returning address}}
}

HeapArray<int>::iterator WontCompileEndLifetime() {
  return HeapArray<int>::WithSize(1u).end();  // expected-error {{returning address}}
}

int& WontCompileIndexLifetime() {
  return HeapArray<int>::WithSize(1u)[0];  // expected-error {{returning reference}}
}

base::span<int> WontCompileSpanLifetime() {
  return HeapArray<int>::WithSize(1u).as_span(); // expected-error {{returning address}}
}

}  // namespace
}  // namespace base
