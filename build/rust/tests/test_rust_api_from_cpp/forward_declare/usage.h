// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_FORWARD_DECLARE_USAGE_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_FORWARD_DECLARE_USAGE_H_

// Forward declaration - the definition is in `definition.h`.
struct TestForwardDeclaredStruct;

int GetValueViaForwardDeclaration(const TestForwardDeclaredStruct& s);
void SetValueViaForwardDeclaration(TestForwardDeclaredStruct& s, int value);
const TestForwardDeclaredStruct& PassThroughForwardDeclaration(
    const TestForwardDeclaredStruct& s);

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_FORWARD_DECLARE_USAGE_H_
