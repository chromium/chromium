// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/legacy_hash.h"

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace legacy {

TEST(LegacyHashTest, CityHashV103) {
  constexpr struct {
    std::string_view input;
    uint64_t output;
    uint64_t output_with_seed;
  } kTestCases[] = {
      {"", 11160318154034397263ull, 14404538258149959151ull},
      {"0123456789", 12631666426400459317ull, 12757304017804637665ull},
      {"hello world", 12386028635079221413ull, 4144044770257928618ull},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input);
    auto bytes = as_bytes(make_span(test_case.input));
    EXPECT_EQ(test_case.output, CityHash64(bytes));
  }
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input);
    auto bytes = as_bytes(make_span(test_case.input));
    EXPECT_EQ(test_case.output_with_seed, CityHash64WithSeed(bytes, 112358));
  }
}

}  // namespace legacy
}  // namespace base
