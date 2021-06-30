// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/no_destructor.h"

#include <string>

namespace base {

#if defined(NCTEST_NODESTRUCTOR_REQUIRES_NONTRIVIAL_DESTRUCTOR) // [r"fatal error: static_assert failed due to requirement '!std::is_trivially_destructible<bool>::value || std::is_same<nullptr_t, base::AllowForTriviallyDestructibleType>::value' \"base::NoDestructor is not needed because the templated class has a trivial destructor\""]

// Attempt to make a NoDestructor for a type with a trivial destructor.
void WontCompile() {
  NoDestructor<bool> nd;
}

#elif defined(NCTEST_NODESTRUCTOR_PARAMETER) // [r"fatal error: static_assert failed due to requirement 'std::is_same<std::string, base::AllowForTriviallyDestructibleType>::value || std::is_same<std::string, nullptr_t>::value' \"AllowForTriviallyDestructibleType is the only valid option for the second template parameter of NoDestructor\""]

// Attempt to make a NoDestructor for a type with an invalid option.
void WontCompile() {
  NoDestructor<std::string, std::string> nd;
}

#endif

}  // namespace base
