// Copyright 2012 The Chromium Authors
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

}  // namespace

}  // namespace base
