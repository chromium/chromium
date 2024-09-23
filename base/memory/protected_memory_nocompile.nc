// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include <string>
#include <type_traits>

#include "base/memory/protected_memory.h"

namespace base {
namespace {
struct NonTriviallyDestructibleData1 {
  std::string data;
};

static_assert(!std::is_trivially_destructible_v<NonTriviallyDestructibleData1>);

struct NonTriviallyDestructibleData2 {
  ~NonTriviallyDestructibleData2() { delete[] data; }

  const char* const data = nullptr;
};

static_assert(!std::is_trivially_destructible_v<NonTriviallyDestructibleData2>);
}  // namespace

void DoNotAcceptDataWhichIsNotTriviallyDestructibleData() {
  base::ProtectedMemory<NonTriviallyDestructibleData1>
      data_1;  // expected-error@base/memory/protected_memory.h:* {{static assertion failed due to requirement 'std::is_trivially_destructible_v<base::(anonymous namespace)::NonTriviallyDestructibleData1>'}}
  base::ProtectedMemory<NonTriviallyDestructibleData2>
      data_2;  // expected-error@base/memory/protected_memory.h:* {{static assertion failed due to requirement 'std::is_trivially_destructible_v<base::(anonymous namespace)::NonTriviallyDestructibleData2>'}}
}

void DoNotAcceptParametersForData() {
  base::ProtectedMemory<int>
      data(2);  // expected-error@base/memory/protected_memory_nocompile.nc:* {{no matching constructor for initialization of 'base::ProtectedMemory<int>'}}
}

}  // namespace base
