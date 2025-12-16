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

// For testing multi-arg PassKey.
class A;
class B;
class C;
class D;

class RestrictedMulti {
 public:
  explicit RestrictedMulti(PassKey<A, B, C>) {}
};

// Test invalid conversions by attempting them from within a valid friend class.
class D {
 public:
  void CannotConstructFromDisjointSingle() {
    // This should fail because PassKey<D> is not part of PassKey<A, B, C>.
    // expected-error@+1 {{no matching constructor for initialization of 'RestrictedMulti'}}
    RestrictedMulti r{PassKey<D>()};
  }
};

class A {
 public:
  void CannotConstructFromDisjointMulti() {
    // This should fail because PassKey<A, D> is not a subset of PassKey<A, B, C>.
    // expected-error@+1 {{no matching constructor for initialization of 'RestrictedMulti'}}
    RestrictedMulti r{PassKey<A, D>(PassKey<A>())};
  }

  void CannotConstructFromSuperset() {
    class RestrictedSmaller {
     public:
      explicit RestrictedSmaller(PassKey<A, B>) {}
    };
    // This should fail because PassKey<A, B, C> is a superset of PassKey<A, B>,
    // not a subset.
    // expected-error@+1 {{no matching constructor for initialization of 'RestrictedSmaller'}}
    RestrictedSmaller r{PassKey<A, B, C>(PassKey<A>())};
  }

  void CannotInstantiateWithNonUniqueTypes() {
    // This should fail because PassKey<A, B, A> has non-unique types.
    // expected-error@*:* {{static assertion failed: PassKey<> arguments must be pairwise unique}}
    PassKey<A, B, A> key{PassKey<A>{}};
  }
};

// Verifies PassKey can't be created from {}.
void PassKeyCannotBeCreatedFromBraces() {
  class A;
  class B;
  class Restricted {
   public:
    explicit Restricted(PassKey<A, B>) {}
  };
  // expected-error@+1 {{no matching constructor for initialization of 'Restricted'}}
  Restricted r({});
}

// Verifies PassKey can't be default-initialized.
void PassKeyCannotBeDefaultInitialized() {
  class A;
  class B;
  // expected-error@+1 {{no matching constructor for initialization of 'PassKey<A, B>'}}
  PassKey<A, B> key;
}

}  // namespace base
