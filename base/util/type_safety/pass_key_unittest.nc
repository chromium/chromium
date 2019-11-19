// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a no-compile test suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/util/type_safety/pass_key.h"

namespace util {

class Manager;

// May not be created without a PassKey.
class Restricted {
 public:
  Restricted(util::PassKey<Manager>) {}
};

int Secret(util::PassKey<Manager>) {
  return 1;
}

#if defined(NCTEST_UNAUTHORIZED_PASS_KEY_IN_INITIALIZER)  // [r"fatal error: calling a private constructor of class 'util::PassKey<util::Manager>'"]

class NotAManager {
 public:
  NotAManager() : restricted_(util::PassKey<Manager>()) {}

 private:
  Restricted restricted_;
};

void WillNotCompile() {
  NotAManager not_a_manager;
}

#elif defined(NCTEST_UNAUTHORIZED_UNIFORM_INITIALIZED_PASS_KEY_IN_INITIALIZER)  // [r"fatal error: calling a private constructor of class 'util::PassKey<util::Manager>'"]

class NotAManager {
 public:
  NotAManager() : restricted_({}) {}

 private:
  Restricted restricted_;
};

void WillNotCompile() {
  NotAManager not_a_manager;
}

#elif defined(NCTEST_UNAUTHORIZED_PASS_KEY_IN_FUNCTION)  // [r"fatal error: calling a private constructor of class 'util::PassKey<util::Manager>'"]

int WillNotCompile() {
  return Secret(util::PassKey<Manager>());
}

#elif defined(NCTEST_UNAUTHORIZED_UNIFORM_INITIALIZATION_WITH_DEDUCED_PASS_KEY_TYPE)  // [r"fatal error: calling a private constructor of class 'util::PassKey<util::Manager>'"]

int WillNotCompile() {
  return Secret({});
}

#elif defined(NCTEST_UNAUTHORIZED_UNIFORM_INITIALIZATION)  // [r"fatal error: calling a private constructor of class 'util::PassKey<util::Manager>'"]

int WillNotCompile() {
  util::PassKey<Manager> key {};
  return Secret(key);
}

#endif

}  // namespace util
