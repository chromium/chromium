// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/util/type_safety/token_type.h"

#include "base/unguessable_token.h"

namespace util {

using FooToken = TokenType<class Foo>;

#if defined(TEST_IMPLICIT_CONVERSION_FAILS)

void WontCompile() {
  FooToken token1;

  // FooToken should not be implicitly convertible to base::UnguessableToken.
  base::UnguessableToken token2(token1);
  base::UnguessableToken token3;
  token3 = token1;
}

#endif

}  // namespace util
