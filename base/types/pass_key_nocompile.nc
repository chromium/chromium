// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a no-compile test suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/types/pass_key.h"

#include <utility>

namespace base {

class Manager;

// May not be created without a PassKey.
class Restricted {
 public:
  Restricted(PassKey<Manager>) {}
};

void Secret(PassKey<Manager>) {}

void CannotConstructFieldFromTemporaryPassKey() {
  class NotAManager {
   public:
    NotAManager() : restricted_(PassKey<Manager>()) {}  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}

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
  Secret(PassKey<Manager>());  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}
}

void CannotConstructPassKeyImplicitly() {
  Secret({});  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}
}

void CannotConstructNamedPassKey() {
  PassKey<Manager> key {};  // expected-error {{calling a private constructor of class 'base::PassKey<base::Manager>'}}
  Secret(key);
}

void CannotCopyNonCopyablePassKey(NonCopyablePassKey<Manager> key) {
  CannotCopyNonCopyablePassKey(key);  // expected-error {{call to deleted constructor of 'NonCopyablePassKey<Manager>'}}
}

void CannotMoveNonCopyablePassKey(NonCopyablePassKey<Manager> key) {
  CannotMoveNonCopyablePassKey(std::move(key));  // expected-error {{call to deleted constructor of 'NonCopyablePassKey<Manager>'}}
}

}  // namespace base
