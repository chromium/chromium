// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using PickerPrefs = IntentChipDisplayPrefs;

class IntentChipDisplayPrefsTest : public testing::Test {
 public:
  IntentChipDisplayPrefsTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(IntentChipDisplayPrefsTest, ResetIntentChipCounter) {
  GURL url("https://www.google.com/abcde");

  TestingProfile profile;

  // Increment counter a few times.
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url),
            PickerPrefs::ChipState::kExpanded);
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url),
            PickerPrefs::ChipState::kExpanded);

  // Reset the count back to 0.
  PickerPrefs::ResetIntentChipCounter(&profile, url);

  // Since the counter is reset, the chip is expanded another 3 times
  // before collapsing.
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url),
            PickerPrefs::ChipState::kExpanded);
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url),
            PickerPrefs::ChipState::kExpanded);
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url),
            PickerPrefs::ChipState::kExpanded);
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url),
            PickerPrefs::ChipState::kCollapsed);
}

// Checks that calling GetChipStateAndIncrementCounter tracks views per-URL
// and collapses the chip after a fixed number of views.
TEST_F(IntentChipDisplayPrefsTest, GetChipStateAndIncrementCounter) {
  using ChipState = PickerPrefs::ChipState;

  GURL url1("https://www.google.com/abcde");
  GURL url2("https://www.google.com/hi");
  GURL url3("https://www.boogle.com/a");

  TestingProfile profile;

  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url1),
            ChipState::kExpanded);
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url2),
            ChipState::kExpanded);
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url2),
            ChipState::kExpanded);
  // Fourth view for a host should be collapsed.
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url1),
            ChipState::kCollapsed);

  // URL on a different host should still be expanded.
  EXPECT_EQ(PickerPrefs::GetChipStateAndIncrementCounter(&profile, url3),
            ChipState::kExpanded);
}
