// Copyright 2019 The Chromium Authors
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

void Secret(base::PassKey<Manager>) {}

void CannotConstructFieldFromTemporaryPassKey() {
  class NotAManager {
   public:
    NotAManager() : restricted_(base::PassKey<Manager>()) {}  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}

   private:
    Restricted restricted_;
  };
}

void CannotConstructFieldFromImplicitPassKey() {
  class NotAManager {
   public:
    NotAManager() : restricted_({}) {}  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}

   private:
    Restricted restricted_;
  };
}

void CannotConstructTemporaryPassKey() {
  Secret(base::PassKey<Manager>());  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}
}

void CannotConstructPassKeyImplicitly() {
  Secret({});  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}
}

void CannotConstructNamedPassKey() {
  base::PassKey<Manager> key {};  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}
  Secret(key);
}

}  // namespace base
