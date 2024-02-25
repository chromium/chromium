// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/is_complete.h"

namespace base {

struct CompleteStruct {};
struct IncompleteStruct;
using Function = void();
using FunctionPtr = void (*)();

template <typename T>
struct SpecializedForInt;

template <>
struct SpecializedForInt<int> {};

static_assert(IsComplete<int>);
static_assert(IsComplete<CompleteStruct>);
static_assert(!IsComplete<IncompleteStruct>);
static_assert(IsComplete<Function>);
static_assert(IsComplete<FunctionPtr>);
static_assert(IsComplete<SpecializedForInt<int>>);
static_assert(!IsComplete<SpecializedForInt<float>>);

}  // namespace base
