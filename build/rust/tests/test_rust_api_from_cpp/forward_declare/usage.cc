// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_rust_api_from_cpp/forward_declare/usage.h"

#include "build/rust/tests/test_rust_api_from_cpp/forward_declare/definition.h"

int GetValueViaForwardDeclaration(const TestForwardDeclaredStruct& s) {
  return s.value;
}

void SetValueViaForwardDeclaration(TestForwardDeclaredStruct& s, int value) {
  s.value = value;
}

const TestForwardDeclaredStruct& PassThroughForwardDeclaration(
    const TestForwardDeclaredStruct& s) {
  return s;
}
