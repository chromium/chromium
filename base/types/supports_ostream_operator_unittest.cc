// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/supports_ostream_operator.h"

#include <stdint.h>

#include <ostream>
#include <string>

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

// A few standard types that definitely support printing.
static_assert(internal::SupportsOstreamOperator<int>,
              "ints should be printable");
static_assert(internal::SupportsOstreamOperator<const char*>,
              "C strings should be printable");
static_assert(internal::SupportsOstreamOperator<std::string>,
              "std::string should be printable");

// Various kinds of enums operator<< support.
static_assert(internal::SupportsOstreamOperator<SimpleEnum>,
              "simple enum should be printable by value");
static_assert(internal::SupportsOstreamOperator<const SimpleEnum&>,
              "simple enum should be printable by const ref");
static_assert(internal::SupportsOstreamOperator<EnumWithExplicitType>,
              "enum with explicit type should be printable by value");
static_assert(internal::SupportsOstreamOperator<const EnumWithExplicitType&>,
              "enum with explicit type should be printable by const ref");
static_assert(!internal::SupportsOstreamOperator<ScopedEnum>,
              "scoped enum should not be printable by value");
static_assert(!internal::SupportsOstreamOperator<const ScopedEnum&>,
              "simple enum should not be printable by const ref");
static_assert(internal::SupportsOstreamOperator<ScopedEnumWithOperator>,
              "scoped enum with operator<< should be printable by value");
static_assert(internal::SupportsOstreamOperator<const ScopedEnumWithOperator&>,
              "scoped enum with operator<< should be printable by const ref");

// operator<< support on structs.
static_assert(!internal::SupportsOstreamOperator<SimpleStruct>,
              "simple struct should not be printable by value");
static_assert(!internal::SupportsOstreamOperator<const SimpleStruct&>,
              "simple struct should not be printable by const ref");
static_assert(internal::SupportsOstreamOperator<StructWithOperator>,
              "struct with operator<< should be printable by value");
static_assert(internal::SupportsOstreamOperator<const StructWithOperator&>,
              "struct with operator<< should be printable by const ref");

}  // namespace
}  // namespace base
