// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/template_util.h"

#include <stdint.h>

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
struct SimpleStruct {};

// is_scoped_enum
TEST(TemplateUtil, IsScopedEnum) {
  static_assert(!is_scoped_enum<int>::value, "");
  static_assert(!is_scoped_enum<SimpleEnum>::value, "");
  static_assert(!is_scoped_enum<EnumWithExplicitType>::value, "");
  static_assert(is_scoped_enum<ScopedEnum>::value, "");
}

TEST(TemplateUtil, RemoveCvRefT) {
  static_assert(std::is_same_v<int, remove_cvref_t<const int>>, "");
  static_assert(std::is_same_v<int, remove_cvref_t<const volatile int>>, "");
  static_assert(std::is_same_v<int, remove_cvref_t<int&>>, "");
  static_assert(std::is_same_v<int, remove_cvref_t<const int&>>, "");
  static_assert(std::is_same_v<int, remove_cvref_t<const volatile int&>>, "");
  static_assert(std::is_same_v<int, remove_cvref_t<int&&>>, "");
  static_assert(
      std::is_same_v<SimpleStruct, remove_cvref_t<const SimpleStruct&>>, "");
  static_assert(std::is_same_v<int*, remove_cvref_t<int*>>, "");

  // Test references and pointers to arrays.
  static_assert(std::is_same_v<int[3], remove_cvref_t<int[3]>>, "");
  static_assert(std::is_same_v<int[3], remove_cvref_t<int(&)[3]>>, "");
  static_assert(std::is_same_v<int(*)[3], remove_cvref_t<int(*)[3]>>, "");

  // Test references and pointers to functions.
  static_assert(std::is_same_v<void(int), remove_cvref_t<void(int)>>, "");
  static_assert(std::is_same_v<void(int), remove_cvref_t<void (&)(int)>>, "");
  static_assert(std::is_same_v<void (*)(int), remove_cvref_t<void (*)(int)>>,
                "");
}

}  // namespace

}  // namespace base
