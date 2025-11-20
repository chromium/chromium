// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "partition_alloc/partition_alloc_base/template_util.h"

namespace partition_alloc::internal::base {
namespace {

enum SimpleEnum { SIMPLE_ENUM };
enum EnumWithExplicitType : uint64_t { ENUM_WITH_EXPLICIT_TYPE };
enum class ScopedEnum { SCOPED_ENUM };
struct SimpleStruct {};

static_assert(!is_scoped_enum<int>::value);
static_assert(!is_scoped_enum<SimpleEnum>::value);
static_assert(!is_scoped_enum<EnumWithExplicitType>::value);
static_assert(is_scoped_enum<ScopedEnum>::value);

}  // namespace
}  // namespace partition_alloc::internal::base
