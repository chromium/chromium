// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <memory>
#include <tuple>  // for std::ignore
#include <type_traits>  // for std::remove_pointer_t

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"

namespace {

struct Producer {};
struct DerivedProducer : Producer {};
struct OtherDerivedProducer : Producer {};
struct Unrelated {};
struct DerivedUnrelated : Unrelated {};
struct PmfTest {
 public:
  int Func(char, double) const { return 11; }
};

#if defined(NCTEST_AUTO_DOWNCAST)  // [r"no viable conversion from 'raw_ptr<Producer>' to 'raw_ptr<DerivedProducer>'"]

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

#elif defined(NCTEST_POINTER_TO_MEMBER) // [r"overload resolution selected deleted operator '->\*'"]

void WontCompile() {
  PmfTest object;
  int (PmfTest::*pmf_func)(char, double) const = &PmfTest::Func;

  raw_ptr<PmfTest> object_ptr = &object;
  std::ignore = object_ptr->*pmf_func;
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

#endif

}  // namespace
