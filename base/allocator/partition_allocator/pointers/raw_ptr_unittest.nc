// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <memory>
#include <tuple>  // for std::ignore
#include <type_traits>  // for std::remove_pointer_t

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

namespace {

struct Producer {};
struct DerivedProducer : Producer {};
struct OtherDerivedProducer : Producer {};
struct Unrelated {};
struct DerivedUnrelated : Unrelated {};

#if defined(NCTEST_INVALID_RAW_PTR_TRAIT)  // [r"Unknown raw_ptr trait\(s\)"]

void WontCompile() {
  constexpr auto InvalidRawPtrTrait = static_cast<base::RawPtrTraits>(-1);
  raw_ptr<int, InvalidRawPtrTrait> p;
}

#elif defined(NCTEST_INVALID_RAW_PTR_TRAIT_OF_MANY)  // [r"Unknown raw_ptr trait\(s\)"]

void WontCompile() {
  constexpr auto InvalidRawPtrTrait = static_cast<base::RawPtrTraits>(-1);
  raw_ptr<int, DisableDanglingPtrDetection | InvalidRawPtrTrait>
      p;
}

#elif defined(NCTEST_AUTO_DOWNCAST)  // [r"no viable conversion from 'raw_ptr<Producer>' to 'raw_ptr<DerivedProducer>'"]

void WontCompile() {
  Producer f;
  raw_ptr<Producer> ptr = &f;
  raw_ptr<DerivedProducer> derived_ptr = ptr;
}

#elif defined(NCTEST_STATIC_DOWNCAST)  // [r"no matching conversion for static_cast from 'raw_ptr<Producer>' to 'raw_ptr<DerivedProducer>'"]

void WontCompile() {
  Producer f;
  raw_ptr<Producer> ptr = &f;
  raw_ptr<DerivedProducer> derived_ptr =
      static_cast<raw_ptr<DerivedProducer>>(ptr);
}

#elif defined(NCTEST_AUTO_REF_DOWNCAST)  // [r"non-const lvalue reference to type 'raw_ptr<DerivedProducer>' cannot bind to a value of unrelated type 'raw_ptr<Producer>'"]

void WontCompile() {
  Producer f;
  raw_ptr<Producer> ptr = &f;
  raw_ptr<DerivedProducer>& derived_ptr = ptr;
}

#elif defined(NCTEST_STATIC_REF_DOWNCAST)  // [r"non-const lvalue reference to type 'raw_ptr<DerivedProducer>' cannot bind to a value of unrelated type 'raw_ptr<Producer>'"]

void WontCompile() {
  Producer f;
  raw_ptr<Producer> ptr = &f;
  raw_ptr<DerivedProducer>& derived_ptr =
      static_cast<raw_ptr<DerivedProducer>&>(ptr);
}

#elif defined(NCTEST_AUTO_DOWNCAST_FROM_RAW) // [r"no viable conversion from 'Producer \*' to 'raw_ptr<DerivedProducer>'"]

void WontCompile() {
  Producer f;
  raw_ptr<DerivedProducer> ptr = &f;
}

#elif defined(NCTEST_UNRELATED_FROM_RAW) // [r"no viable conversion from 'DerivedProducer \*' to 'raw_ptr<Unrelated>'"]

void WontCompile() {
  DerivedProducer f;
  raw_ptr<Unrelated> ptr = &f;
}

#elif defined(NCTEST_UNRELATED_STATIC_FROM_WRAPPED) // [r"static_cast from '\(anonymous namespace\)::DerivedProducer \*' to '\(anonymous namespace\)::Unrelated \*', which are not related by inheritance, is not allowed"]

void WontCompile() {
  DerivedProducer f;
  raw_ptr<DerivedProducer> ptr = &f;
  std::ignore = static_cast<Unrelated*>(ptr);
}

#elif defined(NCTEST_VOID_DEREFERENCE) // [r"indirection requires pointer operand \('raw_ptr<const void>' invalid\)"]

void WontCompile() {
  const char foo[] = "42";
  raw_ptr<const void> ptr = foo;
  std::ignore = *ptr;
}

#elif defined(NCTEST_FUNCTION_POINTER) // [r"raw_ptr<T> doesn't work with this kind of pointee type T"]

void WontCompile() {
  raw_ptr<void(int)> raw_ptr_var;
  std::ignore = raw_ptr_var.get();
}

#elif defined(NCTEST_DANGLING_GSL) // [r"object backing the pointer will be destroyed at the end of the full-expression"]

void WontCompile() {
  [[maybe_unused]] raw_ptr<int> ptr = std::make_unique<int>(2).get();
}

#elif defined(NCTEST_BINDING_RAW_PTR_PARAMETER) // [r"base::Bind\(\) target functor has a parameter of type raw_ptr<T>."]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);

  // Make sure that we are not allowed to bind a function with a raw_ptr<T>
  // parameter type.
  auto callback = base::BindOnce(
      [](raw_ptr<int> ptr) {
      },
      ptr);
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_PLUS_EQUALS_STRUCT) // [r"no viable overloaded '\+='"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  struct {} s;
  ptr += s;
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_MINUS_EQUALS_STRUCT) // [r"no viable overloaded '-='"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  struct {} s;
  ptr -= s;
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_PLUS_STRUCT) // [r"no viable overloaded '\+='"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  struct {} s;
  // Note, operator + exists, but it calls += which doesn't.
  [[maybe_unused]] raw_ptr<int> ptr2 = ptr + s;
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_MINUS_STRUCT) // [r"no viable overloaded '-='"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  struct {} s;
  // Note, operator - exists, but it calls -= which doesn't.
  [[maybe_unused]] raw_ptr<int> ptr2 = ptr - s;
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_PLUS_EQUALS_UINT64) // [r"no viable overloaded '\+='"]

void WontCompile() {
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
  raw_ptr<int> ptr = new int(3);
  ptr += uint64_t{2};
#else
  // Fake error on 64-bit to match the expectation.
  static_assert(false, "no viable overloaded '+='");
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS)
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_MINUS_EQUALS_UINT64) // [r"no viable overloaded '-='"]

void WontCompile() {
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
  raw_ptr<int> ptr = new int(3);
  ptr -= uint64_t{2};
#else
  // Fake error on 64-bit to match the expectation.
  static_assert(false, "no viable overloaded '-='");
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS)
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_PLUS_UINT64) // [r"no viable overloaded '\+='"]

void WontCompile() {
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
  raw_ptr<int> ptr = new int(3);
  // Note, operator + exists, but it calls += which doesn't.
  [[maybe_unused]] raw_ptr<int> ptr2 = ptr + uint64_t{2};
#else
  // Fake error on 64-bit to match the expectation.
  static_assert(false, "no viable overloaded '+='");
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS)
}

#elif defined(NCTEST_BINDING_RAW_PTR_DISALLOW_MINUS_UINT64) // [r"no viable overloaded '-='"]

void WontCompile() {
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
  raw_ptr<int> ptr = new int(3);
  // Note, operator - exists, but it calls -= which doesn't.
  [[maybe_unused]] raw_ptr<int> ptr2 = ptr - uint64_t{2};
#else
  // Fake error on 64-bit to match the expectation.
  static_assert(false, "no viable overloaded '-='");
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS)
}

#elif defined(NCTEST_CROSS_KIND_CONVERSION_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)0U == \(\(partition_alloc::internal::RawPtrTraits\)1U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr = new int(3);
  [[maybe_unused]] raw_ptr<int> ptr2(ptr);
}

#elif defined(NCTEST_CROSS_KIND_CONVERSION_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)1U == \(\(partition_alloc::internal::RawPtrTraits\)2048U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kDummyForTest> ptr = new int(3);
  [[maybe_unused]] raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr2(ptr);
}

#elif defined(NCTEST_CROSS_KIND_MOVE_CONVERSION_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)0U == \(\(partition_alloc::internal::RawPtrTraits\)1U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr = new int(3);
  [[maybe_unused]] raw_ptr<int> ptr2(std::move(ptr));
}

#elif defined(NCTEST_CROSS_KIND_MOVE_CONVERSION_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)1U == \(\(partition_alloc::internal::RawPtrTraits\)2048U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kDummyForTest> ptr = new int(3);
  [[maybe_unused]] raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr2(std::move(ptr));
}

#elif defined(NCTEST_CROSS_KIND_ASSIGNMENT_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)0U == \(\(partition_alloc::internal::RawPtrTraits\)1U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr = new int(3);
  raw_ptr<int> ptr2;
  ptr2 = ptr;
}

#elif defined(NCTEST_CROSS_KIND_ASSIGNMENT_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)1U == \(\(partition_alloc::internal::RawPtrTraits\)2048U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kDummyForTest> ptr = new int(3);
  raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr2;
  ptr2 = ptr;
}

#elif defined(NCTEST_CROSS_KIND_MOVE_ASSIGNMENT_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)0U == \(\(partition_alloc::internal::RawPtrTraits\)1U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr = new int(3);
  raw_ptr<int> ptr2;
  ptr2 = std::move(ptr);
}

#elif defined(NCTEST_CROSS_KIND_MOVE_ASSIGNMENT_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)1U == \(\(partition_alloc::internal::RawPtrTraits\)2048U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  raw_ptr<int, base::RawPtrTraits::kDummyForTest> ptr = new int(3);
  raw_ptr<int, base::RawPtrTraits::kMayDangle> ptr2;
  ptr2 = std::move(ptr);
}

// TODO(tsepez): enable once enable_pointer_arithmetic_trait_check=true.
#elif defined(DISABLED_NCTEST_BAN_PTR_INCREMENT) // [r"static assertion failed due to requirement '.*IsPtrArithmeticAllowed.*'"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  ptr++;
}

#elif defined(DISABLED_NCTEST_BAN_PTR_DECREMENT) // [r"static assertion failed due to requirement '.*IsPtrArithmeticAllowed.*'"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  ptr--;
}

#elif defined(DISABLED_NCTEST_BAN_PTR_ADDITION) // [r"static assertion failed due to requirement '.*IsPtrArithmeticAllowed.*'"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  raw_ptr<int> ptr2 = ptr + 1;
}

#elif defined(DISABLED_NCTEST_BAN_PTR_SUBTRACTION) // [r"static assertion failed due to requirement '.*IsPtrArithmeticAllowed.*'"]

void WontCompile() {
  raw_ptr<int> ptr = new int(3);
  raw_ptr<int> ptr2 = ptr - 1;
}

#elif defined(DISABLED_NCTEST_BAN_PTR_INDEX) // [r"static assertion failed due to requirement '.*IsPtrArithmeticAllowed.*'"]

int WontCompile() {
  raw_ptr<int> ptr = new int(3);
  return ptr[1];
}

#endif

}  // namespace
