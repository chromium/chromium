// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/template_util.h"

#include <string>
#include <type_traits>

#include "base/containers/flat_tree.h"
#include "base/test/move_only_int.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

enum SimpleEnum { SIMPLE_ENUM };
enum EnumWithExplicitType : uint64_t { ENUM_WITH_EXPLICIT_TYPE };
enum class ScopedEnum { SCOPED_ENUM };
enum class ScopedEnumWithOperator { SCOPED_ENUM_WITH_OPERATOR };
std::ostream& operator<<(std::ostream& os, ScopedEnumWithOperator v) {
  return os;
}
struct SimpleStruct {};
struct StructWithOperator {};
std::ostream& operator<<(std::ostream& os, const StructWithOperator& v) {
  return os;
}
struct StructWithToString {
  std::string ToString() const { return ""; }
};

// is_non_const_reference<Type>
static_assert(!is_non_const_reference<int>::value, "IsNonConstReference");
static_assert(!is_non_const_reference<const int&>::value,
              "IsNonConstReference");
static_assert(is_non_const_reference<int&>::value, "IsNonConstReference");

// A few standard types that definitely support printing.
static_assert(internal::SupportsOstreamOperator<int>::value,
              "ints should be printable");
static_assert(internal::SupportsOstreamOperator<const char*>::value,
              "C strings should be printable");
static_assert(internal::SupportsOstreamOperator<std::string>::value,
              "std::string should be printable");

// Various kinds of enums operator<< support.
static_assert(internal::SupportsOstreamOperator<SimpleEnum>::value,
              "simple enum should be printable by value");
static_assert(internal::SupportsOstreamOperator<const SimpleEnum&>::value,
              "simple enum should be printable by const ref");
static_assert(internal::SupportsOstreamOperator<EnumWithExplicitType>::value,
              "enum with explicit type should be printable by value");
static_assert(
    internal::SupportsOstreamOperator<const EnumWithExplicitType&>::value,
    "enum with explicit type should be printable by const ref");
static_assert(!internal::SupportsOstreamOperator<ScopedEnum>::value,
              "scoped enum should not be printable by value");
static_assert(!internal::SupportsOstreamOperator<const ScopedEnum&>::value,
              "simple enum should not be printable by const ref");
static_assert(internal::SupportsOstreamOperator<ScopedEnumWithOperator>::value,
              "scoped enum with operator<< should be printable by value");
static_assert(
    internal::SupportsOstreamOperator<const ScopedEnumWithOperator&>::value,
    "scoped enum with operator<< should be printable by const ref");

// operator<< support on structs.
static_assert(!internal::SupportsOstreamOperator<SimpleStruct>::value,
              "simple struct should not be printable by value");
static_assert(!internal::SupportsOstreamOperator<const SimpleStruct&>::value,
              "simple struct should not be printable by const ref");
static_assert(internal::SupportsOstreamOperator<StructWithOperator>::value,
              "struct with operator<< should be printable by value");
static_assert(
    internal::SupportsOstreamOperator<const StructWithOperator&>::value,
    "struct with operator<< should be printable by const ref");

// .ToString() support on structs.
static_assert(!internal::SupportsToString<SimpleStruct>::value,
              "simple struct value doesn't support .ToString()");
static_assert(!internal::SupportsToString<const SimpleStruct&>::value,
              "simple struct const ref doesn't support .ToString()");
static_assert(internal::SupportsToString<StructWithToString>::value,
              "struct with .ToString() should be printable by value");
static_assert(internal::SupportsToString<const StructWithToString&>::value,
              "struct with .ToString() should be printable by const ref");

// base::is_trivially_copyable
class TrivialCopy {
 public:
  TrivialCopy(int d) : data_(d) {}

 protected:
  int data_;
};

class TrivialCopyButWithDestructor : public TrivialCopy {
 public:
  TrivialCopyButWithDestructor(int d) : TrivialCopy(d) {}
  ~TrivialCopyButWithDestructor() { data_ = 0; }
};

static_assert(base::is_trivially_copyable<TrivialCopy>::value,
              "TrivialCopy should be detected as trivially copyable");
static_assert(!base::is_trivially_copyable<TrivialCopyButWithDestructor>::value,
              "TrivialCopyButWithDestructor should not be detected as "
              "trivially copyable");

class NoCopy {
 public:
  NoCopy(const NoCopy&) = delete;
};

static_assert(
    !base::is_trivially_copy_constructible<std::vector<NoCopy>>::value,
    "is_trivially_copy_constructible<std::vector<T>> must be compiled.");

using TrueT = std::true_type;
using FalseT = std::false_type;

// bool_constant
static_assert(std::is_same<TrueT, bool_constant<true>>::value, "");
static_assert(std::is_same<FalseT, bool_constant<false>>::value, "");

struct True {
  static constexpr bool value = true;
};

struct False {
  static constexpr bool value = false;
};

// conjunction
static_assert(conjunction<>::value, "");
static_assert(conjunction<TrueT>::value, "");
static_assert(!conjunction<FalseT>::value, "");

static_assert(conjunction<TrueT, TrueT>::value, "");
static_assert(!conjunction<TrueT, FalseT>::value, "");
static_assert(!conjunction<FalseT, TrueT>::value, "");
static_assert(!conjunction<FalseT, FalseT>::value, "");

static_assert(conjunction<TrueT, TrueT, TrueT>::value, "");
static_assert(!conjunction<TrueT, TrueT, FalseT>::value, "");
static_assert(!conjunction<TrueT, FalseT, TrueT>::value, "");
static_assert(!conjunction<TrueT, FalseT, FalseT>::value, "");
static_assert(!conjunction<FalseT, TrueT, TrueT>::value, "");
static_assert(!conjunction<FalseT, TrueT, FalseT>::value, "");
static_assert(!conjunction<FalseT, FalseT, TrueT>::value, "");
static_assert(!conjunction<FalseT, FalseT, FalseT>::value, "");

static_assert(conjunction<True>::value, "");
static_assert(!conjunction<False>::value, "");

// disjunction
static_assert(!disjunction<>::value, "");
static_assert(disjunction<TrueT>::value, "");
static_assert(!disjunction<FalseT>::value, "");

static_assert(disjunction<TrueT, TrueT>::value, "");
static_assert(disjunction<TrueT, FalseT>::value, "");
static_assert(disjunction<FalseT, TrueT>::value, "");
static_assert(!disjunction<FalseT, FalseT>::value, "");

static_assert(disjunction<TrueT, TrueT, TrueT>::value, "");
static_assert(disjunction<TrueT, TrueT, FalseT>::value, "");
static_assert(disjunction<TrueT, FalseT, TrueT>::value, "");
static_assert(disjunction<TrueT, FalseT, FalseT>::value, "");
static_assert(disjunction<FalseT, TrueT, TrueT>::value, "");
static_assert(disjunction<FalseT, TrueT, FalseT>::value, "");
static_assert(disjunction<FalseT, FalseT, TrueT>::value, "");
static_assert(!disjunction<FalseT, FalseT, FalseT>::value, "");

static_assert(disjunction<True>::value, "");
static_assert(!disjunction<False>::value, "");

// negation
static_assert(!negation<TrueT>::value, "");
static_assert(negation<FalseT>::value, "");

static_assert(!negation<True>::value, "");
static_assert(negation<False>::value, "");

static_assert(negation<negation<TrueT>>::value, "");
static_assert(!negation<negation<FalseT>>::value, "");

// is_invocable
TEST(TemplateUtil, IsInvocable) {
  struct Base {};
  struct Derived : Base {};

  struct Implicit {
    Implicit(int) {}
  };

  struct Explicit {
    explicit Explicit(int) {}
  };

  struct CallableWithBaseButNotWithInt {
    int operator()(int) = delete;
    int operator()(Base) { return 42; }
  };

  {
    using Fp = void (*)(Base&, int);
    static_assert(is_invocable<Fp, Base&, int>::value, "");
    static_assert(is_invocable<Fp, Derived&, int>::value, "");
    static_assert(!is_invocable<Fp, const Base&, int>::value, "");
    static_assert(!is_invocable<Fp>::value, "");
    static_assert(!is_invocable<Fp, Base&>::value, "");
  }
  {
    // Function reference
    using Fp = void (&)(Base&, int);
    static_assert(is_invocable<Fp, Base&, int>::value, "");
    static_assert(is_invocable<Fp, Derived&, int>::value, "");
    static_assert(!is_invocable<Fp, const Base&, int>::value, "");
    static_assert(!is_invocable<Fp>::value, "");
    static_assert(!is_invocable<Fp, Base&>::value, "");
  }
  {
    // Function object
    using Fn = CallableWithBaseButNotWithInt;
    static_assert(is_invocable<Fn, Base>::value, "");
    static_assert(!is_invocable<Fn, int>::value, "");
  }
  {
    // Check that the conversion to the return type is properly checked
    using Fn = int (*)(int);
    static_assert(is_invocable_r<Implicit, Fn, int>::value, "");
    static_assert(is_invocable_r<double, Fn, int>::value, "");
    static_assert(is_invocable_r<const volatile void, Fn, int>::value, "");
    static_assert(!is_invocable_r<Explicit, Fn, int>::value, "");

    static_assert(is_invocable_r<Implicit, Fn, double>::value, "");
    static_assert(!is_invocable_r<double, Fn, std::string>::value, "");
    static_assert(is_invocable_r<const volatile void, Fn, double>::value, "");
    static_assert(!is_invocable_r<Explicit, Fn, double>::value, "");
  }
}

// is_scoped_enum
TEST(TemplateUtil, IsScopedEnum) {
  static_assert(!is_scoped_enum<int>::value, "");
  static_assert(!is_scoped_enum<SimpleEnum>::value, "");
  static_assert(!is_scoped_enum<EnumWithExplicitType>::value, "");
  static_assert(is_scoped_enum<ScopedEnum>::value, "");
}

TEST(TemplateUtil, RemoveCvRefT) {
  static_assert(std::is_same<int, remove_cvref_t<const int>>::value, "");
  static_assert(std::is_same<int, remove_cvref_t<const volatile int>>::value,
                "");
  static_assert(std::is_same<int, remove_cvref_t<int&>>::value, "");
  static_assert(std::is_same<int, remove_cvref_t<const int&>>::value, "");
  static_assert(std::is_same<int, remove_cvref_t<const volatile int&>>::value,
                "");
  static_assert(std::is_same<int, remove_cvref_t<int&&>>::value, "");
  static_assert(
      std::is_same<SimpleStruct, remove_cvref_t<const SimpleStruct&>>::value,
      "");
  static_assert(std::is_same<int*, remove_cvref_t<int*>>::value, "");

  // Test references and pointers to arrays.
  static_assert(std::is_same<int[3], remove_cvref_t<int[3]>>::value, "");
  static_assert(std::is_same<int[3], remove_cvref_t<int(&)[3]>>::value, "");
  static_assert(std::is_same<int(*)[3], remove_cvref_t<int(*)[3]>>::value, "");

  // Test references and pointers to functions.
  static_assert(std::is_same<void(int), remove_cvref_t<void(int)>>::value, "");
  static_assert(std::is_same<void(int), remove_cvref_t<void (&)(int)>>::value,
                "");
  static_assert(
      std::is_same<void (*)(int), remove_cvref_t<void (*)(int)>>::value, "");
}

TEST(TemplateUtil, IsConstantEvaluated) {
  // base::is_constant_evaluated() should return whether it is evaluated as part
  // of a constant expression.
  static_assert(is_constant_evaluated(), "");
  EXPECT_FALSE(is_constant_evaluated());
}

}  // namespace

}  // namespace base
