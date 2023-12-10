// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"

namespace base {

class Base : public RefCounted<Base> {
 private:
  friend class RefCounted<Base>;

  ~Base() = default;
};

class ThreadSafeBase : public RefCountedThreadSafe<ThreadSafeBase> {
 private:
  friend class RefCountedThreadSafe<ThreadSafeBase>;

  ~ThreadSafeBase() = default;
};

class DeleteOnSequenceBase
    : public RefCountedDeleteOnSequence<DeleteOnSequenceBase> {
 private:
  friend class RefCountedDeleteOnSequence<DeleteOnSequenceBase>;
  friend class DeleteHelper<DeleteOnSequenceBase>;

  ~DeleteOnSequenceBase() = default;
};

void AdoptRefToZeroStart() {
  class InitialRefCountIsZero : public RefCounted<InitialRefCountIsZero> {
   public:
    InitialRefCountIsZero() = default;

   private:
    friend class RefCounted<InitialRefCountIsZero>;

    ~InitialRefCountIsZero() = default;
  };

  AdoptRef(new InitialRefCountIsZero());  // expected-error@*:* {{Use AdoptRef only if the reference count starts from one.}}
}

void WrongRefcountBaseClass() {
  class Derived : public RefCounted<Base> {
   private:
    friend class RefCounted<Derived>;

    ~Derived() = default;
  };

  scoped_refptr<Derived> ptr;  // expected-error@*:* {{T implements RefCounted<U>, but U is not a base of T.}}
}

void WrongRefcountThreadsafeBaseClass() {
  class Derived : public RefCountedThreadSafe<ThreadSafeBase> {
   private:
    friend class RefCountedThreadSafe<Derived>;

    ~Derived() = default;
  };

  scoped_refptr<Derived> ptr;  // expected-error@*:* {{T implements RefCountedThreadSafe<U>, but U is not a base of T.}}
}

void WrongRefcountDeleteOnSequenceBaseClass() {
  class DeleteOnSequenceDerived
      : public RefCountedDeleteOnSequence<DeleteOnSequenceBase> {
   private:
    friend class RefCountedDeleteOnSequence<DeleteOnSequenceDerived>;
    friend class DeleteHelper<DeleteOnSequenceDerived>;

    ~DeleteOnSequenceDerived() = default;
  };

  scoped_refptr<DeleteOnSequenceDerived> ptr;  // expected-error@*:* {{T implements RefCountedDeleteOnSequence<U>, but U is not a base of T.}}
}

void SubclassOverridesRefcountPreference() {
  class Derived : public Base {
   public:
    REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  };

  scoped_refptr<Derived> ptr;  // expected-error@*:* {{It's unsafe to override the ref count preference. Please remove REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE from subclasses.}}
}

void SubclassOverridesRefcountPreferenceThreadsafe() {
  class Derived : public ThreadSafeBase {
   public:
    REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  };

  scoped_refptr<Derived> ptr;  // expected-error@*:* {{It's unsafe to override the ref count preference. Please remove REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE from subclasses.}}
}

void SubclassOverridesRefcountPreferenceDeleteOnSequence() {
  class Derived : public DeleteOnSequenceBase {
   public:
    REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  };

  scoped_refptr<Derived> ptr;  // expected-error@*:* {{It's unsafe to override the ref count preference. Please remove REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE from subclasses.}}
}

}  // namespace base
