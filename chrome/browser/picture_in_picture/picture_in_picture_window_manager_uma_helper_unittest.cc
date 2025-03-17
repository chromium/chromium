// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager_uma_helper.h"

#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPictureInPictureTotalTimeHistogram[] =
    "Media.PictureInPicture.Window.TotalTime";

class PictureInPictureWindowManagerUmaHelperTest : public testing::Test {
 public:
  PictureInPictureWindowManagerUmaHelperTest() = default;

  void SetUp() override {
    clock_.SetNowTicks(base::TimeTicks::Now());
    uma_helper_.SetClockForTest(&clock_);
  }

  base::SimpleTestTickClock* clock() { return &clock_; }

  PictureInPictureWindowManagerUmaHelper& uma_helper() { return uma_helper_; }

  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceTestStart(
      const std::string& name) {
    return histogram_tester_.GetHistogramSamplesSinceCreation(name);
  }

 private:
  base::SimpleTestTickClock clock_;
  PictureInPictureWindowManagerUmaHelper uma_helper_;
  base::HistogramTester histogram_tester_;
};

}  // anonymous namespace

TEST_F(PictureInPictureWindowManagerUmaHelperTest,
       EnterAndClosePip_DoesCommit) {
  uma_helper().MaybeRecordPictureInPictureChanged(true);

  clock()->Advance(base::Milliseconds(3000));
  uma_helper().MaybeRecordPictureInPictureChanged(false);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(kPictureInPictureTotalTimeHistogram));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(3000));
}

TEST_F(PictureInPictureWindowManagerUmaHelperTest,
       EnterAndRepeatedlyClosePip_DoesCommit) {
  uma_helper().MaybeRecordPictureInPictureChanged(true);

  clock()->Advance(base::Milliseconds(3000));
  uma_helper().MaybeRecordPictureInPictureChanged(false);
  clock()->Advance(base::Milliseconds(2000));
  uma_helper().MaybeRecordPictureInPictureChanged(false);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(kPictureInPictureTotalTimeHistogram));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(3000));
}

TEST_F(PictureInPictureWindowManagerUmaHelperTest,
       CloseAndRepeatedlyEnterPip_DoesNotCommit) {
  uma_helper().MaybeRecordPictureInPictureChanged(false);

  clock()->Advance(base::Milliseconds(3000));
  uma_helper().MaybeRecordPictureInPictureChanged(true);
  clock()->Advance(base::Milliseconds(2000));
  uma_helper().MaybeRecordPictureInPictureChanged(true);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(kPictureInPictureTotalTimeHistogram));
  EXPECT_EQ(0, samples->TotalCount());
}

TEST_F(PictureInPictureWindowManagerUmaHelperTest,
       CloseRepeatedlyEnterThenClosePip_DoesCommit) {
  uma_helper().MaybeRecordPictureInPictureChanged(false);

  clock()->Advance(base::Milliseconds(3000));
  uma_helper().MaybeRecordPictureInPictureChanged(true);
  clock()->Advance(base::Milliseconds(2000));
  uma_helper().MaybeRecordPictureInPictureChanged(true);
  clock()->Advance(base::Milliseconds(1000));
  uma_helper().MaybeRecordPictureInPictureChanged(false);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(kPictureInPictureTotalTimeHistogram));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
}
