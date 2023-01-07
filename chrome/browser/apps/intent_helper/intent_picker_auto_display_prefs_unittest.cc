// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_prefs.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using PickerPrefs = IntentPickerAutoDisplayPrefs;

class IntentPickerAutoDisplayPrefsTest : public testing::Test {
 public:
  IntentPickerAutoDisplayPrefsTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(IntentPickerAutoDisplayPrefsTest, GetPlatform) {
  GURL url1("https://www.google.com/abcde");
  GURL url2("https://www.google.com/hi");
  GURL url3("https://www.boogle.com/a");

  TestingProfile profile;

  EXPECT_EQ(PickerPrefs::Platform::kNone,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url1));
  EXPECT_EQ(PickerPrefs::Platform::kNone,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url2));
  EXPECT_EQ(PickerPrefs::Platform::kNone,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url3));

  // Update platform to a value and check value has updated.
  PickerPrefs::UpdatePlatformForTablets(&profile, url1,
                                        PickerPrefs::Platform::kArc);
  EXPECT_EQ(PickerPrefs::Platform::kArc,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url1));
  // Url with the same host should also have updated value.
  EXPECT_EQ(PickerPrefs::Platform::kArc,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url2));

  // Url with a different host should have original value.
  EXPECT_EQ(PickerPrefs::Platform::kNone,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url3));

  // Update platform and check value has updated.
  PickerPrefs::UpdatePlatformForTablets(&profile, url2,
                                        PickerPrefs::Platform::kChrome);
  EXPECT_EQ(PickerPrefs::Platform::kChrome,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url1));
  // Url with the same host should also have updated value.
  EXPECT_EQ(PickerPrefs::Platform::kChrome,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url2));

  // Url with a different host should have original value.
  EXPECT_EQ(PickerPrefs::Platform::kNone,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url3));

  // Update platform and check value has updated.
  PickerPrefs::UpdatePlatformForTablets(&profile, url3,
                                        PickerPrefs::Platform::kArc);
  // Url with different hosts should have original value.
  EXPECT_EQ(PickerPrefs::Platform::kChrome,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url1));
  EXPECT_EQ(PickerPrefs::Platform::kChrome,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url2));

  // Url value should be updated.
  EXPECT_EQ(PickerPrefs::Platform::kArc,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url3));

  PickerPrefs::UpdatePlatformForTablets(&profile, url3,
                                        PickerPrefs::Platform::kNone);
  // Url value should be updated.
  EXPECT_EQ(PickerPrefs::Platform::kNone,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url3));
  // Url with different hosts should have original value.
  EXPECT_EQ(PickerPrefs::Platform::kChrome,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url1));
  EXPECT_EQ(PickerPrefs::Platform::kChrome,
            PickerPrefs::GetLastUsedPlatformForTablets(&profile, url2));
}

// Checks that calling IncrementCounter twice for a particular URL causes
// the UI to no longer be auto-displayed for that URL.
TEST_F(IntentPickerAutoDisplayPrefsTest, ShouldAutoDisplayUi) {
  GURL url1("https://www.google.com/abcde");
  GURL url2("https://www.google.com/hi");
  GURL url3("https://www.boogle.com/a");

  TestingProfile profile;

  EXPECT_TRUE(PickerPrefs::ShouldAutoDisplayUi(&profile, url1));
  PickerPrefs::IncrementPickerUICounter(&profile, url1);
  EXPECT_TRUE(PickerPrefs::ShouldAutoDisplayUi(&profile, url1));
  PickerPrefs::IncrementPickerUICounter(&profile, url1);
  EXPECT_FALSE(PickerPrefs::ShouldAutoDisplayUi(&profile, url1));

  // Should return false for a different URL on the same host.
  EXPECT_FALSE(PickerPrefs::ShouldAutoDisplayUi(&profile, url2));
  // Should return true for a different host.
  EXPECT_TRUE(PickerPrefs::ShouldAutoDisplayUi(&profile, url3));
}

TEST_F(IntentPickerAutoDisplayPrefsTest, ResetIntentChipCounter) {
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
TEST_F(IntentPickerAutoDisplayPrefsTest, GetChipStateAndIncrementCounter) {
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
