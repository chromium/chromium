// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/same_as_any.h"

namespace base {
namespace {

// Typical usage.
static_assert(SameAsAny<int, char, int, float>);

// Can be invoked with no typelist, but nothing will match, not even `void`.
static_assert(!SameAsAny<void>);

// `void` will match normally, however.
static_assert(SameAsAny<void, void>);
static_assert(SameAsAny<void, void, int>);

// Can use duplicate types in list.
static_assert(SameAsAny<void, void, void>);
static_assert(!SameAsAny<int, void, void>);

// Pointer types work also.
static_assert(SameAsAny<int*, void, int, int*>);

// Constness matters.
static_assert(!SameAsAny<const int*, void, int, int*>);

}  // namespace
}  // namespace base
