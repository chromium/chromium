// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_enum_reader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

TEST(HistogramEnumReaderTest, SanityChecks) {
  {
    // NOTE: This results in a dependency on the enums.xml file, but to
    // otherwise inject content would circumvent a lot of the logic of the
    // method and add additional complexity. "Boolean" is hopefully a pretty
    // stable enum.
    absl::optional<HistogramEnumEntryMap> results =
        ReadEnumFromEnumsXml("Boolean");
    ASSERT_TRUE(results);
    EXPECT_EQ("False", results->at(0));
    EXPECT_EQ("True", results->at(1));
  }

  {
    absl::optional<HistogramEnumEntryMap> results =
        ReadEnumFromEnumsXml("TheWorstNameForAnEnum");
    ASSERT_FALSE(results);
  }
}

}  // namespace base
