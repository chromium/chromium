// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_variants_reader.h"

#include <optional>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(HistogramVariantsReaderTest, SanityChecks) {
  // NOTE: This results in a dependency on a histogram.xml file, but to
  // otherwise inject content would circumvent a lot of the logic of the
  // method and add additional complexity.
  //
  // The test file with the "TestToken" variant can be found at:
  //   `//tools/metrics/histograms/test_data/histograms.xml`
  std::optional<HistogramVariantsEntryMap> results =
      ReadVariantsFromHistogramsXml("TestToken", "test_data",
                                    /*from_metadata=*/false);
  ASSERT_TRUE(results);
  EXPECT_THAT(*results, testing::UnorderedElementsAre(
                            std::make_pair("Variant1", "Label1"),
                            std::make_pair("Variant2", "Label2")));

  results = ReadVariantsFromHistogramsXml("TheWorstNameForVariants",
                                          "test_data", /*from_metadata=*/false);
  ASSERT_FALSE(results);
}

}  // namespace base
