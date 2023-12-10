// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "partition_alloc/pointers/raw_ref.h"

namespace {

// Using distinct enum types causes distinct `raw_ptr` template instantiations,
// so we get assertion failures below where we expect.
enum TypeA {};
enum TypeB {};
enum TypeC {};
enum TypeD {};

using DanglingRefA = raw_ref<TypeA, base::RawPtrTraits::kMayDangle>;
using DanglingRefB = raw_ref<TypeB, base::RawPtrTraits::kMayDangle>;
using DanglingRefC = raw_ref<TypeC, base::RawPtrTraits::kMayDangle>;
using DanglingRefD = raw_ref<TypeD, base::RawPtrTraits::kMayDangle>;

void CrossKindConversionFromMayDangle() {
  // Conversions may add the `kMayDangle` trait, but not remove it.
  TypeA a;
  TypeB b;
  TypeC c;
  TypeD d;
  DanglingRefA ref_a1(a);
  DanglingRefB ref_b1(b);
  DanglingRefC ref_c1(c);
  DanglingRefD ref_d1(d);
  raw_ref<TypeA> ref_a2 = ref_a1;             // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeA, 5>::Traits | RawPtrTraits::kMayDangle)'}}
  raw_ref<TypeB> ref_b2(ref_b1);              // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeB, 5>::Traits | RawPtrTraits::kMayDangle)'}}
  raw_ref<TypeC> ref_c2 = std::move(ref_c1);  // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeC, 5>::Traits | RawPtrTraits::kMayDangle)'}}
  raw_ref<TypeD> ref_d2(std::move(ref_d1));   // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeD, 5>::Traits | RawPtrTraits::kMayDangle)'}}
}

void CrossKindConversionFromDummy() {
  // Only the `kMayDangle` trait can change in an implicit conversion.
  TypeA a;
  TypeB b;
  TypeC c;
  TypeD d;
  raw_ref<TypeA, base::RawPtrTraits::kDummyForTest> ref_a1(a);
  raw_ref<TypeB, base::RawPtrTraits::kDummyForTest> ref_b1(b);
  raw_ref<TypeC, base::RawPtrTraits::kDummyForTest> ref_c1(c);
  raw_ref<TypeD, base::RawPtrTraits::kDummyForTest> ref_d1(d);
  DanglingRefA ref_a2 = ref_a1;             // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeA, 2052>::Traits | RawPtrTraits::kMayDangle)'}}
  DanglingRefB ref_b3(ref_b1);              // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeB, 2052>::Traits | RawPtrTraits::kMayDangle)'}}
  DanglingRefC ref_c2 = std::move(ref_c1);  // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeC, 2052>::Traits | RawPtrTraits::kMayDangle)'}}
  DanglingRefD ref_d2(std::move(ref_d1));   // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeD, 2052>::Traits | RawPtrTraits::kMayDangle)'}}
}

}  // namespace
