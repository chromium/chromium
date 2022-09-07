// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a no-compile test suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/types/pass_key.h"

namespace base {

class Manager;

// May not be created without a PassKey.
class Restricted {
 public:
  Restricted(base::PassKey<Manager>) {}
};

int Secret(base::PassKey<Manager>) {
  return 1;
}

#if defined(NCTEST_UNAUTHORIZED_PASS_KEY_IN_INITIALIZER)  // [r"fatal error: calling a private constructor of class 'base::PassKey<base::Manager>'"]

class NotAManager {
 public:
  NotAManager() : restricted_(base::PassKey<Manager>()) {}

 private:
  Restricted restricted_;
};

void WillNotCompile() {
  NotAManager not_a_manager;
}

#elif defined(NCTEST_UNAUTHORIZED_UNIFORM_INITIALIZED_PASS_KEY_IN_INITIALIZER)  // [r"fatal error: calling a private constructor of class 'base::PassKey<base::Manager>'"]

class NotAManager {
 public:
  NotAManager() : restricted_({}) {}

 private:
  Restricted restricted_;
};

void WillNotCompile() {
  NotAManager not_a_manager;
}

#elif defined(NCTEST_UNAUTHORIZED_PASS_KEY_IN_FUNCTION)  // [r"fatal error: calling a private constructor of class 'base::PassKey<base::Manager>'"]

int WillNotCompile() {
  return Secret(base::PassKey<Manager>());
}

#elif defined(NCTEST_UNAUTHORIZED_UNIFORM_INITIALIZATION_WITH_DEDUCED_PASS_KEY_TYPE)  // [r"fatal error: calling a private constructor of class 'base::PassKey<base::Manager>'"]

int WillNotCompile() {
  return Secret({});
}

#elif defined(NCTEST_UNAUTHORIZED_UNIFORM_INITIALIZATION)  // [r"fatal error: calling a private constructor of class 'base::PassKey<base::Manager>'"]

int WillNotCompile() {
  base::PassKey<Manager> key {};
  return Secret(key);
}

#endif

}  // namespace base
