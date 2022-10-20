// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/memory/weak_ptr.h"

namespace base {

struct Producer : SupportsWeakPtr<Producer> {};
struct DerivedProducer : Producer {};
struct OtherDerivedProducer : Producer {};
struct MultiplyDerivedProducer : Producer,
                                 SupportsWeakPtr<MultiplyDerivedProducer> {};
struct Unrelated {};
struct DerivedUnrelated : Unrelated {};

#if defined(NCTEST_AUTO_DOWNCAST)  // [r"no viable conversion from 'WeakPtr<Producer>' to 'WeakPtr<DerivedProducer>'"]

void WontCompile() {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  WeakPtr<DerivedProducer> derived_ptr = ptr;
}

#elif defined(NCTEST_STATIC_DOWNCAST)  // [r"no matching conversion for static_cast from 'WeakPtr<Producer>' to 'WeakPtr<DerivedProducer>'"]

void WontCompile() {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  WeakPtr<DerivedProducer> derived_ptr =
      static_cast<WeakPtr<DerivedProducer> >(ptr);
}

#elif defined(NCTEST_AUTO_REF_DOWNCAST)  // [r"fatal error: non-const lvalue reference to type 'WeakPtr<DerivedProducer>' cannot bind to a value of unrelated type 'WeakPtr<Producer>'"]

void WontCompile() {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  WeakPtr<DerivedProducer>& derived_ptr = ptr;
}

#elif defined(NCTEST_STATIC_REF_DOWNCAST)  // [r"fatal error: non-const lvalue reference to type 'WeakPtr<DerivedProducer>' cannot bind to a value of unrelated type 'WeakPtr<Producer>'"]

void WontCompile() {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  WeakPtr<DerivedProducer>& derived_ptr =
      static_cast<WeakPtr<DerivedProducer>&>(ptr);
}

#elif defined(NCTEST_STATIC_ASWEAKPTR_DOWNCAST)  // [r"no matching function"]

void WontCompile() {
  Producer f;
  WeakPtr<DerivedProducer> ptr =
      SupportsWeakPtr<Producer>::StaticAsWeakPtr<DerivedProducer>(&f);
}

#elif defined(NCTEST_UNSAFE_HELPER_DOWNCAST)  // [r"no viable conversion from 'WeakPtr<base::Producer>' to 'WeakPtr<DerivedProducer>'"]

void WontCompile() {
  Producer f;
  WeakPtr<DerivedProducer> ptr = AsWeakPtr(&f);
}

#elif defined(NCTEST_UNSAFE_INSTANTIATED_HELPER_DOWNCAST)  // [r"no matching function"]

void WontCompile() {
  Producer f;
  WeakPtr<DerivedProducer> ptr = AsWeakPtr<DerivedProducer>(&f);
}

#elif defined(NCTEST_UNSAFE_WRONG_INSANTIATED_HELPER_DOWNCAST)  // [r"no viable conversion from 'WeakPtr<base::Producer>' to 'WeakPtr<DerivedProducer>'"]

void WontCompile() {
  Producer f;
  WeakPtr<DerivedProducer> ptr = AsWeakPtr<Producer>(&f);
}

#elif defined(NCTEST_UNSAFE_HELPER_CAST)  // [r"no viable conversion from 'WeakPtr<base::DerivedProducer>' to 'WeakPtr<OtherDerivedProducer>'"]

void WontCompile() {
  DerivedProducer f;
  WeakPtr<OtherDerivedProducer> ptr = AsWeakPtr(&f);
}

#elif defined(NCTEST_UNSAFE_INSTANTIATED_HELPER_SIDECAST)  // [r"fatal error: no matching function for call to 'AsWeakPtr'"]

void WontCompile() {
  DerivedProducer f;
  WeakPtr<OtherDerivedProducer> ptr = AsWeakPtr<OtherDerivedProducer>(&f);
}

#elif defined(NCTEST_UNSAFE_WRONG_INSTANTIATED_HELPER_SIDECAST)  // [r"no viable conversion from 'WeakPtr<base::DerivedProducer>' to 'WeakPtr<OtherDerivedProducer>'"]

void WontCompile() {
  DerivedProducer f;
  WeakPtr<OtherDerivedProducer> ptr = AsWeakPtr<DerivedProducer>(&f);
}

#elif defined(NCTEST_UNRELATED_HELPER)  // [r"no viable conversion from 'WeakPtr<base::DerivedProducer>' to 'WeakPtr<Unrelated>'"]

void WontCompile() {
  DerivedProducer f;
  WeakPtr<Unrelated> ptr = AsWeakPtr(&f);
}

#elif defined(NCTEST_UNRELATED_INSTANTIATED_HELPER)  // [r"no matching function"]

void WontCompile() {
  DerivedProducer f;
  WeakPtr<Unrelated> ptr = AsWeakPtr<Unrelated>(&f);
}

#elif defined(NCTEST_COMPLETELY_UNRELATED_HELPER)  // [r"fatal error: static assertion failed due to requirement 'std::is_base_of<base::internal::SupportsWeakPtrBase, base::Unrelated>::value': AsWeakPtr argument must inherit from SupportsWeakPtr"]

void WontCompile() {
  Unrelated f;
  WeakPtr<Unrelated> ptr = AsWeakPtr(&f);
}

#elif defined(NCTEST_DERIVED_COMPLETELY_UNRELATED_HELPER)  // [r"fatal error: static assertion failed due to requirement 'std::is_base_of<base::internal::SupportsWeakPtrBase, base::DerivedUnrelated>::value': AsWeakPtr argument must inherit from SupportsWeakPtr"]

void WontCompile() {
  DerivedUnrelated f;
  WeakPtr<Unrelated> ptr = AsWeakPtr(&f);
}

#elif defined(NCTEST_AMBIGUOUS_ANCESTORS)  // [r"fatal error: no matching function for call to 'AsWeakPtrImpl'"]

void WontCompile() {
  MultiplyDerivedProducer f;
  WeakPtr<MultiplyDerivedProducer> ptr = AsWeakPtr(&f);
}

#elif defined(NCTEST_GETMUTABLEWEAKPTR_CONST_T)  // [r"fatal error: no matching member function for call to 'GetMutableWeakPtr'"]

void WontCompile() {
  Unrelated unrelated;
  const WeakPtrFactory<const Unrelated> factory(&unrelated);
  factory.GetMutableWeakPtr();
}

#elif defined(NCTEST_GETMUTABLEWEAKPTR_NOT_T)  // [r"fatal error: no matching member function for call to 'GetMutableWeakPtr'"]

void WontCompile() {
  DerivedUnrelated derived_unrelated;
  const WeakPtrFactory<DerivedUnrelated> factory(&derived_unrelated);
  factory.GetMutableWeakPtr<Unrelated>();
}

#endif

}
