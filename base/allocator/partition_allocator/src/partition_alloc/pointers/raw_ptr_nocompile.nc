// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "partition_alloc/pointers/raw_ptr.h"
#include "partition_alloc/buildflags.h"

namespace {

// Using distinct enum types causes distinct `raw_ptr` template instantiations,
// so we get assertion failures below where we expect.
enum TypeA {};
enum TypeB {};

void UnknownTraits() {
  constexpr auto InvalidRawPtrTrait = static_cast<base::RawPtrTraits>(-1);
  raw_ptr<TypeA, InvalidRawPtrTrait> ptr_a;                                // expected-error@*:* {{Unknown raw_ptr trait(s)}}
  raw_ptr<TypeB, DisableDanglingPtrDetection | InvalidRawPtrTrait> ptr_b;  // expected-error@*:* {{Unknown raw_ptr trait(s)}}
}

void DifferentTypeAssignment() {
  struct Unrelated {};
  struct Producer {} p;
  struct DerivedProducer : public Producer {} dp;
  raw_ptr<Producer> ptr_p = &p;
  raw_ptr<DerivedProducer> ptr_dp1 = &dp;

  // Conversion
  raw_ptr<DerivedProducer> ptr_dp2 = ptr_p;  // expected-error {{no viable conversion from 'raw_ptr<Producer>' to 'raw_ptr<DerivedProducer>'}}
  raw_ptr<DerivedProducer> ptr_dp3 =
      static_cast<raw_ptr<DerivedProducer>>(ptr_p);  // expected-error {{no matching conversion for static_cast from 'raw_ptr<Producer>' to 'raw_ptr<DerivedProducer>'}}
  raw_ptr<DerivedProducer> ptr_dp4 = &p;             // expected-error {{no viable conversion from 'struct Producer *' to 'raw_ptr<DerivedProducer>'}}
  raw_ptr<Unrelated> ptr_u1 = &dp;                   // expected-error {{no viable conversion from 'struct DerivedProducer *' to 'raw_ptr<Unrelated>'}}

  // Reference binding
  raw_ptr<DerivedProducer>& ptr_dp5 = ptr_p;  // expected-error {{non-const lvalue reference to type 'raw_ptr<DerivedProducer>' cannot bind to a value of unrelated type 'raw_ptr<Producer>'}}
  raw_ptr<DerivedProducer>& ptr_dp6 =
      static_cast<raw_ptr<DerivedProducer>&>(ptr_p);  // expected-error {{non-const lvalue reference to type 'raw_ptr<DerivedProducer>' cannot bind to a value of unrelated type 'raw_ptr<Producer>'}}

  // Casting
  auto* ptr_u2 = static_cast<Unrelated*>(ptr_dp1);  // expected-error@*:* {{static_cast from 'DerivedProducer *' to 'Unrelated *', which are not related by inheritance, is not allowed}}
}

void DereferenceVoidPtr() {
  constexpr char kFoo[] = "42";
  raw_ptr<const void> ptr = kFoo;
  *ptr;  // expected-error {{indirection requires pointer operand ('raw_ptr<const void>' invalid)}}
}

void FunctionPointerType() {
  raw_ptr<void(int)> ptr;  // expected-error@*:* {{raw_ptr<T> doesn't work with this kind of pointee type T}}
}

void Dangling() {
  [[maybe_unused]] raw_ptr<int> ptr = std::make_unique<int>(2).get();  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
}

void BindRawPtrParam() {
  // `raw_ptr` is not intended to be used as a function param type, so trying to
  // bind to a function with a `raw_ptr<T>` param should error out.
  raw_ptr<int> ptr = new int(3);
  base::BindOnce([](raw_ptr<int> ptr) {}, ptr);  // expected-error@*:* {{Use T* or T& instead of raw_ptr<T> for function parameters, unless you must mark the parameter as MayBeDangling<T>.}}
}

void PointerArithmetic() {
  using PtrCanDoArithmetic =
      raw_ptr<int, base::RawPtrTraits::kAllowPtrArithmetic>;

  PtrCanDoArithmetic ptr1 = new int(3);
  struct {} s;
  ptr1 += s;                           // expected-error@*:* {{no viable overloaded '+='}}
  ptr1 -= s;                           // expected-error@*:* {{no viable overloaded '-='}}
  PtrCanDoArithmetic ptr2 = ptr1 + s;  // expected-error@*:* {{no matching function for call to 'Advance'}}
  ptr2 = ptr1 - s;                     // expected-error@*:* {{no matching function for call to 'Retreat'}}

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  ptr1 += uint64_t{2};        // expected-error@*:* {{no viable overloaded '+='}}
  ptr1 -= uint64_t{2};        // expected-error@*:* {{no viable overloaded '-='}}
  ptr2 = ptr1 + uint64_t{2};  // expected-error@*:* {{no matching function for call to 'Advance'}}
  ptr2 = ptr1 - uint64_t{2};  // expected-error@*:* {{no matching function for call to 'Retreat'}}
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
}

#if PA_BUILDFLAG(ENABLE_POINTER_ARITHMETIC_TRAIT_CHECK)
void PointerArithmeticDisabled() {
  raw_ptr<TypeA> ptr_a1 = new TypeA();
  ptr_a1++;                            // expected-error@*:* {{cannot increment raw_ptr unless AllowPtrArithmetic trait is present.}}
  ptr_a1--;                            // expected-error@*:* {{cannot decrement raw_ptr unless AllowPtrArithmetic trait is present.}}
  ++ptr_a1;                            // expected-error@*:* {{cannot increment raw_ptr unless AllowPtrArithmetic trait is present.}}
  --ptr_a1;                            // expected-error@*:* {{cannot decrement raw_ptr unless AllowPtrArithmetic trait is present.}}
  raw_ptr<TypeA> ptr_a2 = ptr_a1 + 1;  // expected-error@*:* {{cannot add to raw_ptr unless AllowPtrArithmetic trait is present.}}
  ptr_a2 = ptr_a1 - 1;                 // expected-error@*:* {{cannot subtract from raw_ptr unless AllowPtrArithmetic trait is present.}}
  raw_ptr<TypeB> ptr_b1 = new TypeB();
  raw_ptr<TypeB> ptr_b2 = 1 + ptr_b1;  // expected-error@*:* {{cannot add to raw_ptr unless AllowPtrArithmetic trait is present.}}
  ptr_b2 - ptr_b1;                     // expected-error@*:* {{cannot subtract raw_ptrs unless AllowPtrArithmetic trait is present.}}
}

void Indexing() {
  raw_ptr<int> ptr = new int(3);
  [[maybe_unused]] int val = ptr[1];  // expected-error@*:* {{cannot index raw_ptr unless AllowPtrArithmetic trait is present.}}
}
#endif

using DanglingPtrA = raw_ptr<TypeA, base::RawPtrTraits::kMayDangle>;
using DanglingPtrB = raw_ptr<TypeB, base::RawPtrTraits::kMayDangle>;

void CrossKindConversionFromMayDangle() {
  // Conversions may add the `kMayDangle` trait, but not remove it.
  DanglingPtrA ptr_a1 = new TypeA();
  DanglingPtrB ptr_b1 = new TypeB();
  raw_ptr<TypeA> ptr_a2 = ptr_a1;             // expected-error {{no viable conversion from 'raw_ptr<[...], base::RawPtrTraits::kMayDangle aka 1>' to 'raw_ptr<[...], (default) RawPtrTraits::kEmpty aka 0>'}}
  raw_ptr<TypeA> ptr_a3(ptr_a1);              // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeA, partition_alloc::internal::RawPtrTraits::kMayDangle>::Traits | RawPtrTraits::kMayDangle)'}}
  raw_ptr<TypeA> ptr_a4 = std::move(ptr_a1);  // expected-error {{no viable conversion from '__libcpp_remove_reference_t<raw_ptr<TypeA, partition_alloc::internal::RawPtrTraits::kMayDangle> &>' (aka 'base::raw_ptr<(anonymous namespace)::TypeA, partition_alloc::internal::RawPtrTraits::kMayDangle>') to 'raw_ptr<TypeA>'}}
  raw_ptr<TypeB> ptr_b2(std::move(ptr_b1));   // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeB, partition_alloc::internal::RawPtrTraits::kMayDangle>::Traits | RawPtrTraits::kMayDangle)'}}
}

void CrossKindConversionFromDummy() {
  // Only the `kMayDangle` trait can change in an implicit conversion.
  raw_ptr<TypeA, base::RawPtrTraits::kDummyForTest> ptr_a1 = new TypeA();
  raw_ptr<TypeB, base::RawPtrTraits::kDummyForTest> ptr_b1 = new TypeB();
  DanglingPtrA ptr_a2 = ptr_a1;             // expected-error {{no viable conversion from 'raw_ptr<[...], base::RawPtrTraits::kDummyForTest aka 2048>' to 'raw_ptr<[...], base::RawPtrTraits::kMayDangle aka 1>'}}
  DanglingPtrA ptr_a3(ptr_a1);              // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeA, partition_alloc::internal::RawPtrTraits::kDummyForTest>::Traits | RawPtrTraits::kMayDangle)'}}
  DanglingPtrA ptr_a4 = std::move(ptr_a1);  // expected-error {{no viable conversion from '__libcpp_remove_reference_t<raw_ptr<TypeA, partition_alloc::internal::RawPtrTraits::kDummyForTest> &>' (aka 'base::raw_ptr<(anonymous namespace)::TypeA, partition_alloc::internal::RawPtrTraits::kDummyForTest>') to 'DanglingPtrA' (aka 'raw_ptr<TypeA, base::RawPtrTraits::kMayDangle>')}}
  DanglingPtrB ptr_b2(std::move(ptr_b1));   // expected-error@*:* {{static assertion failed due to requirement 'Traits == (raw_ptr<(anonymous namespace)::TypeB, partition_alloc::internal::RawPtrTraits::kDummyForTest>::Traits | RawPtrTraits::kMayDangle)'}}
}

void CantStorePointerObtainedFromEphemeralRawAddr() {
   int v = 123;
   raw_ptr<int> ptr = &v;
   int** wont_work = &ptr.AsEphemeralRawAddr();  // expected-error {{temporary whose address is used as value of local variable 'wont_work' will be destroyed at the end of the full-expression}}
   *wont_work = nullptr;
}

void CantStoreReferenceObtainedFromEphemeralRawAddr() {
   int v = 123;
   raw_ptr<int> ptr = &v;
   int*& wont_work = ptr.AsEphemeralRawAddr();  // expected-error {{temporary bound to local reference 'wont_work' will be destroyed at the end of the full-expression}}
   wont_work = nullptr;
}

}  // namespace
