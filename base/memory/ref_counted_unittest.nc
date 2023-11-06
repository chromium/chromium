// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"

namespace base {

class InitialRefCountIsZero : public base::RefCounted<InitialRefCountIsZero> {
 public:
  InitialRefCountIsZero() {}
 private:
  friend class base::RefCounted<InitialRefCountIsZero>;
  ~InitialRefCountIsZero() {}
};

#if defined(NCTEST_ADOPT_REF_TO_ZERO_START)  // [r"fatal error: static assertion failed due to requirement 'std::is_same_v<base::subtle::StartRefCountFromOneTag, base::subtle::StartRefCountFromZeroTag>': Use AdoptRef only if the reference count starts from one\."]

void WontCompile() {
  AdoptRef(new InitialRefCountIsZero());
}

#endif

#if defined(NCTEST_WRONG_REFCOUNT_BASE_CLASS)  // [r"fatal error: static assertion failed due to requirement 'std::is_base_of_v<base::Foo, base::Bar>': T implements RefCounted<U>, but U is not a base of T\."]

class Foo : public base::RefCounted<Foo> {
 private:
  friend class base::RefCounted<Foo>;
  ~Foo() {}
};

class Bar : public base::RefCounted<Foo> {
 private:
  friend class base::RefCounted<Bar>;
  ~Bar() {}
};

void WontCompile() {
  scoped_refptr<Bar> ptr;
}

#endif

#if defined(NCTEST_WRONG_REFCOUNT_THREADSAFE_BASE_CLASS)  // [r"fatal error: static assertion failed due to requirement 'std::is_base_of_v<base::Foo, base::Bar>': T implements RefCountedThreadSafe<U>, but U is not a base of T\."]

class Foo : public base::RefCountedThreadSafe<Foo> {
 private:
  friend class base::RefCountedThreadSafe<Foo>;
  ~Foo() {}
};

class Bar : public base::RefCountedThreadSafe<Foo> {
 private:
  friend class base::RefCountedThreadSafe<Bar>;
  ~Bar() {}
};

void WontCompile() {
  scoped_refptr<Bar> ptr;
}

#endif

#if defined(NCTEST_WRONG_REFCOUNT_ON_SEQUENCE_BASE_CLASS)  // [r"fatal error: static assertion failed due to requirement 'std::is_base_of_v<base::Foo, base::Bar>': T implements RefCountedDeleteOnSequence<U>, but U is not a base of T\."]

class Foo : public base::RefCountedDeleteOnSequence<Foo> {
 private:
  friend class base::RefCountedDeleteOnSequence<Foo>;
  friend class base::DeleteHelper<Foo>;
  ~Foo() {}
};

class Bar : public base::RefCountedDeleteOnSequence<Foo> {
 private:
  friend class base::RefCountedDeleteOnSequence<Bar>;
  friend class base::DeleteHelper<Bar>;
  ~Bar() {}
};

void WontCompile() {
  scoped_refptr<Bar> ptr;
}

#endif

#if defined(NCTEST_SUBCLASS_OVERRIDES_REFCOUNT_PREFERENCE)  // [r"fatal error: static assertion failed due to requirement .*: It's unsafe to override the ref count preference\. Please remove REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE from subclasses\."]

class Base : public base::RefCounted<Base> {
 protected:
  friend class base::RefCounted<Base>;
  ~Base() {}
};

class Derived : public Base {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
};

void WontCompile() {
  scoped_refptr<Derived> ptr;
}

#endif

#if defined(NCTEST_SUBCLASS_OVERRIDES_REFCOUNT_PREFERENCE_THREADSAFE)  // [r"fatal error: static assertion failed due to requirement .*: It's unsafe to override the ref count preference\. Please remove REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE from subclasses\."]

class Base : public base::RefCountedThreadSafe<Base> {
 protected:
  friend class base::RefCountedThreadSafe<Base>;
  ~Base() {}
};

class Derived : public Base {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
};

void WontCompile() {
  scoped_refptr<Derived> ptr;
}

#endif

#if defined(NCTEST_SUBCLASS_OVERRIDES_REFCOUNT_PREFERENCE_SEQUENCE)  // [r"fatal error: static assertion failed due to requirement .*: It's unsafe to override the ref count preference\. Please remove REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE from subclasses\."]

class Base : public base::RefCountedDeleteOnSequence<Base> {
 protected:
  friend class base::RefCountedDeleteOnSequence<Base>;
  friend class base::DeleteHelper<Base>;
  ~Base() {}
};

class Derived : public Base {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
};

void WontCompile() {
  scoped_refptr<Derived> ptr;
}

#endif


}  // namespace base
