// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/ranges_manager.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using testing::UnorderedElementsAre;

TEST(RangesManagerTest, GetOrRegisterCanonicalRanges) {
  RangesManager ranges_manager;

  // Create some BucketRanges. We call |ResetChecksum| to calculate and set
  // their checksums. Checksums are used to validate integrity (and test for
  // non-equivalence) and should be computed after a BucketRanges is fully
  // initialized. Note that BucketRanges are initialized with 0 for all ranges,
  // i.e., all buckets will be [0, 0).
  BucketRanges* ranges1 = new BucketRanges(3);
  ranges1->ResetChecksum();
  BucketRanges* ranges2 = new BucketRanges(4);
  ranges2->ResetChecksum();

  // Register new ranges.
  EXPECT_EQ(ranges1, ranges_manager.GetOrRegisterCanonicalRanges(ranges1));
  EXPECT_EQ(ranges2, ranges_manager.GetOrRegisterCanonicalRanges(ranges2));
  EXPECT_THAT(ranges_manager.GetBucketRanges(),
              UnorderedElementsAre(ranges1, ranges2));

  // Register |ranges1| again. The registered BucketRanges set should not change
  // as |ranges1| is already registered.
  EXPECT_EQ(ranges1, ranges_manager.GetOrRegisterCanonicalRanges(ranges1));
  EXPECT_THAT(ranges_manager.GetBucketRanges(),
              UnorderedElementsAre(ranges1, ranges2));

  // Make sure |ranges1| still exists, and is the same as what we expect (all
  // ranges are 0).
  ASSERT_EQ(3u, ranges1->size());
  EXPECT_EQ(0, ranges1->range(0));
  EXPECT_EQ(0, ranges1->range(1));
  EXPECT_EQ(0, ranges1->range(2));

  // Register a new |ranges3| that is equivalent to |ranges1| (same ranges). If
  // GetOrRegisterCanonicalRanges() returns a different object than the param
  // (as asserted here), we are responsible for deleting the object (below).
  BucketRanges* ranges3 = new BucketRanges(3);
  ranges3->ResetChecksum();
  ASSERT_EQ(ranges1, ranges_manager.GetOrRegisterCanonicalRanges(ranges3));
  delete ranges3;
  EXPECT_THAT(ranges_manager.GetBucketRanges(),
              UnorderedElementsAre(ranges1, ranges2));
}

TEST(RangesManagerTest, ReleaseBucketRangesOnDestroy) {
  std::unique_ptr<RangesManager> ranges_manager =
      std::make_unique<RangesManager>();

  // Create a BucketRanges. We call |ResetChecksum| to calculate and set its
  // checksum. Checksums are used to validate integrity (and test for
  // non-equivalence) and should be computed after a BucketRanges is fully
  // initialized. Note that BucketRanges are initialized with 0 for all ranges,
  // i.e., all buckets will be [0, 0).
  BucketRanges* ranges = new BucketRanges(1);
  ranges->ResetChecksum();

  // Register new range.
  EXPECT_EQ(ranges, ranges_manager->GetOrRegisterCanonicalRanges(ranges));
  EXPECT_THAT(ranges_manager->GetBucketRanges(), UnorderedElementsAre(ranges));

  // Explicitly destroy |ranges_manager|.
  ranges_manager.reset();

  // LeakSanitizer (lsan) bots will verify that |ranges| will be properly
  // released after destroying |ranges_manager|.
}

}  // namespace base
