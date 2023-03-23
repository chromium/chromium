// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/dump_without_crashing.h"

#include "base/hash/hash.h"
#include "base/location.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

class DumpWithoutCrashingTest : public testing::Test {
 public:
  static void DummyDumpWithoutCrashing() { number_of_dump_calls_++; }

  static int number_of_dump_calls() { return number_of_dump_calls_; }

 protected:
  void SetUp() override {
    SetDumpWithoutCrashingFunction(
        &DumpWithoutCrashingTest::DummyDumpWithoutCrashing);
    number_of_dump_calls_ = 0;
  }

  void TearDown() override {
    SetDumpWithoutCrashingFunction(nullptr);
    ClearMapsForTesting();
  }

  // Set override.
  ScopedMockClockOverride clock_;

  const base::HistogramTester histogram_tester_;
  const base::Location location1_ = FROM_HERE;
  const base::Location location2_ = FROM_HERE;
  const size_t unique_identifier1_ =
      base::FastHash("DumpWithoutCrashingFirstTest");
  const size_t unique_identifier2_ =
      base::FastHash("DumpWithoutCrashingSecondTest");

 private:
  static int number_of_dump_calls_;
};

int DumpWithoutCrashingTest::number_of_dump_calls_ = 0;

TEST_F(DumpWithoutCrashingTest, DumpWithoutCrashingWithLocation) {
  EXPECT_EQ(0, DumpWithoutCrashingTest::number_of_dump_calls());

  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 0);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 0);

  // The first call to DumpWithoutCrashing will always capture the dump and
  // will return true
  EXPECT_TRUE(DumpWithoutCrashing(location1_, base::Seconds(1)));
  EXPECT_EQ(1, DumpWithoutCrashingTest::number_of_dump_calls());

  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 0);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 1);

  // If DumpWithoutCrashing is called within 1 second, expected result is false.
  EXPECT_FALSE(DumpWithoutCrashing(location1_, base::Seconds(1)));
  EXPECT_EQ(1, DumpWithoutCrashingTest::number_of_dump_calls());

  // For testing, the time for capturing dump again is 1 second and if the
  // function is called after 1 second, it will return true.
  clock_.Advance(Seconds(2));
  EXPECT_TRUE(DumpWithoutCrashing(location1_, base::Seconds(1)));
  EXPECT_EQ(2, DumpWithoutCrashingTest::number_of_dump_calls());

  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 1);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 2);

  EXPECT_TRUE(DumpWithoutCrashing(location2_, base::Seconds(1)));
  EXPECT_EQ(3, DumpWithoutCrashingTest::number_of_dump_calls());

  EXPECT_FALSE(DumpWithoutCrashing(location2_, base::Seconds(1)));
  EXPECT_EQ(3, DumpWithoutCrashingTest::number_of_dump_calls());

  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 2);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 3);
}

TEST_F(DumpWithoutCrashingTest, DumpWithoutCrashingWithLocationAndUniqueId) {
  EXPECT_EQ(0, DumpWithoutCrashingTest::number_of_dump_calls());

  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 0);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 0);

  // Test the variant of DumpWithoutCrashingWithUniqueId where the function
  // takes a location and unique id.
  EXPECT_TRUE(DumpWithoutCrashingWithUniqueId(unique_identifier1_, location1_,
                                              base::Seconds(1)));
  EXPECT_EQ(1, DumpWithoutCrashingTest::number_of_dump_calls());

  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 0);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 1);

  // If DumpWithoutCrashingWithUniqueId called within 1 second, expected result
  // is false.
  EXPECT_FALSE(DumpWithoutCrashingWithUniqueId(unique_identifier1_, location1_,
                                               base::Seconds(1)));
  EXPECT_EQ(1, DumpWithoutCrashingTest::number_of_dump_calls());

  // For testing, the time for capturing dump again is 1 second and if the
  // function is called after 1 second, it will return true.
  clock_.Advance(Seconds(2));

  EXPECT_TRUE(DumpWithoutCrashingWithUniqueId(unique_identifier1_, location1_,
                                              base::Seconds(1)));
  EXPECT_EQ(2, DumpWithoutCrashingTest::number_of_dump_calls());

  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 1);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 2);

  EXPECT_TRUE(DumpWithoutCrashingWithUniqueId(unique_identifier2_, location2_,
                                              base::Seconds(1)));
  EXPECT_EQ(3, DumpWithoutCrashingTest::number_of_dump_calls());
  EXPECT_FALSE(DumpWithoutCrashingWithUniqueId(unique_identifier2_, location2_,
                                               base::Seconds(1)));
  EXPECT_EQ(3, DumpWithoutCrashingTest::number_of_dump_calls());
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kThrottled, 2);
  histogram_tester_.ExpectBucketCount("Stability.DumpWithoutCrashingStatus",
                                      DumpWithoutCrashingStatus::kUploaded, 3);
}

}  // namespace debug
}  // namespace base
