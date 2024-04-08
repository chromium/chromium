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
  base::ProtectedMemory<NonTriviallyDestructibleData1,
                        false /*ConstructLazily*/>
      data_1_eager;  // expected-error@base/memory/protected_memory.h:* {{static assertion failed due to requirement 'std::is_trivially_destructible_v<base::(anonymous namespace)::NonTriviallyDestructibleData1>'}}
  base::ProtectedMemory<NonTriviallyDestructibleData2,
                        false /*ConstructLazily*/>
      data_2_eager;  // expected-error@base/memory/protected_memory.h:* {{static assertion failed due to requirement 'std::is_trivially_destructible_v<base::(anonymous namespace)::NonTriviallyDestructibleData2>'}}

  base::ProtectedMemory<NonTriviallyDestructibleData1, true /*ConstructLazily*/>
      data_1_lazy;  // expected-error@base/memory/protected_memory.h:* {{static assertion failed due to requirement 'std::is_trivially_destructible_v<base::(anonymous namespace)::NonTriviallyDestructibleData1>'}}
  base::ProtectedMemory<NonTriviallyDestructibleData2, true /*ConstructLazily*/>
      data_2_lazy;  // expected-error@base/memory/protected_memory.h:* {{static assertion failed due to requirement 'std::is_trivially_destructible_v<base::(anonymous namespace)::NonTriviallyDestructibleData2>'}}
}

void DoNotAcceptParametersForDataWhichIsLazilyConstructible() {
  base::ProtectedMemory<int, true /*ConstructLazily*/> data(
      2);  // expected-error@base/memory/protected_memory_nocompile.nc:* {{no matching constructor for initialization of 'base::ProtectedMemory<int, true>'}}
}

}  // namespace base
