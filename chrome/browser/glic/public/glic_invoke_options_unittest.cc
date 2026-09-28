// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_invoke_options.h"

#include <set>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicInvokeOptionsTest, EveryInvokeErrorHasADistinctString) {
  std::set<std::string_view> seen;
  // 0 is reserved for success in metrics, so values start at 1.
  for (int value = 1; value <= static_cast<int>(GlicInvokeError::kMaxValue);
       ++value) {
    const auto error = static_cast<GlicInvokeError>(value);
    const std::string_view description = GlicInvokeErrorToString(error);
    EXPECT_FALSE(description.empty()) << "No description for error " << value;
    EXPECT_TRUE(seen.insert(description).second)
        << "Duplicate description \"" << description << "\" for error "
        << value;
  }
}

}  // namespace
}  // namespace glic
