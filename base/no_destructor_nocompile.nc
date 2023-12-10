// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/no_destructor.h"

namespace base {

void WontCompileWithTrivialTypes() {
  // NoDestructor should not be used with trivial types; trivial types can
  // simply be directly declared as globals.
  static NoDestructor<bool> x;  // expected-error@*:* {{static assertion failed due to requirement '!(std::is_trivially_constructible_v<bool> && std::is_trivially_destructible_v<bool>)'}}
}

struct TypeWithUserConstructor {
  TypeWithUserConstructor() {}
};

void WontCompileWithTriviallyDestructibleTypes() {
  // NoDestructor should only be used for non-trivially destructible types;
  // they should be declared as function-level statics instead.
  static NoDestructor<TypeWithUserConstructor> x;  // expected-error@*:* {{static assertion failed due to requirement '!std::is_trivially_destructible_v<base::TypeWithUserConstructor>'}}
}

}  // namespace base
