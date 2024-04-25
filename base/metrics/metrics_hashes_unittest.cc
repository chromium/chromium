// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/metrics/metrics_hashes.h"

#include <stddef.h>
#include <stdint.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Make sure our ID hashes are the same as what we see on the server side.
TEST(MetricsHashesTest, HashMetricName) {
  // The cases must match those in //tools/metrics/ukm/codegen_test.py.
  static const struct {
    std::string input;
    std::string output;
  } cases[] = {
      {"Back", "0x0557fa923dcee4d0"},
      {"NewTab", "0x290eb683f96572f1"},
      {"Forward", "0x67d2f6740a8eaebf"},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    uint64_t hash = HashMetricName(cases[i].input);
    std::string hash_hex = base::StringPrintf("0x%016" PRIx64, hash);
    EXPECT_EQ(cases[i].output, hash_hex);
  }
}

TEST(MetricsHashesTest, HashMetricNameAs32Bits) {
  // The cases must match those in //tools/metrics/ukm/codegen_test.py.
  static const struct {
    std::string input;
    std::string output;
  } cases[] = {
      {"Back", "0x0557fa92"},
      {"NewTab", "0x290eb683"},
      {"Forward", "0x67d2f674"},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    uint32_t hash = HashMetricNameAs32Bits(cases[i].input);
    std::string hash_hex = base::StringPrintf("0x%08" PRIx32, hash);
    EXPECT_EQ(cases[i].output, hash_hex);
  }
}

}  // namespace metrics
