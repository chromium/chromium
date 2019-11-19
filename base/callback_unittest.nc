// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/callback.h"

namespace base {

class Parent {
};

class Child : Parent {
};

#if defined(NCTEST_EQUALS_REQUIRES_SAMETYPE)  // [r"fatal error: invalid operands to binary expression \('RepeatingCallback<void \(\)>' and 'RepeatingCallback<int \(\)>'\)"]

// Attempting to call comparison function on two callbacks of different type.
//
// This should be a compile time failure because each callback type should be
// considered distinct.
void WontCompile() {
  RepeatingCallback<void()> c1;
  RepeatingCallback<int()> c2;
  c1 == c2;
}

#elif defined(NCTEST_CONSTRUCTION_FROM_SUBTYPE)  // [r"fatal error: no viable conversion from 'RepeatingCallback<base::Parent \(\)>' to 'RepeatingCallback<base::Child \(\)>'"]

// Construction of RepeatingCallback<A> from RepeatingCallback<B> if A is
// supertype of B.
//
// While this is technically safe, most people aren't used to it when coding
// C++ so if this is happening, it is almost certainly an error.
void WontCompile() {
  RepeatingCallback<Parent()> cb_a;
  RepeatingCallback<Child()> cb_b = cb_a;
}

#elif defined(NCTEST_ASSIGNMENT_FROM_SUBTYPE)  // [r"fatal error: no viable overloaded '='"]

// Assignment of RepeatingCallback<A> from RepeatingCallback<B> if A is
// supertype of B. See explanation for NCTEST_CONSTRUCTION_FROM_SUBTYPE.
void WontCompile() {
  RepeatingCallback<Parent()> cb_a;
  RepeatingCallback<Child()> cb_b;
  cb_a = cb_b;
}

#endif

}  // namespace base
