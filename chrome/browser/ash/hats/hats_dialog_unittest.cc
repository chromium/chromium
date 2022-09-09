// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "hats_dialog.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(HatsDialogTest, HandleClientTriggeredAction) {
  // Client asks to close the window
  EXPECT_TRUE(HatsDialog::HandleClientTriggeredAction("close", "hist-name"));
  // There was an unhandled error, close the window
  EXPECT_TRUE(HatsDialog::HandleClientTriggeredAction(
      "survey-loading-error-12345", "a-suffix"));
  // Client sent an invalid action, ignore it
  EXPECT_FALSE(HatsDialog::HandleClientTriggeredAction("Invalid", "hist-name"));

  // Set up the histogram tester
  base::HistogramTester histogram_tester;
  std::string histogram("Browser.ChromeOS.HatsSatisfaction.General");
  histogram_tester.ExpectTotalCount(histogram, 0);

  EXPECT_FALSE(HatsDialog::HandleClientTriggeredAction("smiley-selected-4",
                                                       "full-histogram-name"));

  // Ensure we logged the right metric
  // For the example above, it means adding 1 entry in the bucket for score=4
  std::vector<base::Bucket> expected_buckets{{4, 1}};
  EXPECT_EQ(histogram_tester.GetAllSamples("full-histogram-name"),
            expected_buckets);
}

}  // namespace ash
