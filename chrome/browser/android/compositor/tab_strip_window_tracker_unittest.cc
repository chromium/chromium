// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_window_tracker.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {
namespace {

class TabStripWindowTrackerTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(TabStripWindowTrackerTest, CountsLiveWindows) {
  auto first = std::make_unique<TabStripWindowTracker>(&profile_);
  auto second = std::make_unique<TabStripWindowTracker>(&profile_);
  EXPECT_FALSE(first->had_existing_window());
  EXPECT_TRUE(second->had_existing_window());

  // Destroyed trackers no longer count.
  first.reset();
  second.reset();
  TabStripWindowTracker third(&profile_);
  EXPECT_FALSE(third.had_existing_window());
}

TEST_F(TabStripWindowTrackerTest, CountsAreIndependentPerProfile) {
  Profile* otr_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  TabStripWindowTracker regular(&profile_);
  TabStripWindowTracker otr(otr_profile);
  EXPECT_FALSE(regular.had_existing_window());
  EXPECT_FALSE(otr.had_existing_window());

  TabStripWindowTracker second_otr(otr_profile);
  EXPECT_TRUE(second_otr.had_existing_window());
}

TEST_F(TabStripWindowTrackerTest, ProfileDestroyedBeforeTracker) {
  Profile* otr_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto tracker = std::make_unique<TabStripWindowTracker>(otr_profile);
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile);
  ASSERT_FALSE(profile_.HasPrimaryOTRProfile());

  // Must not touch the destroyed Profile.
  tracker.reset();

  // A new OTR profile starts with a fresh count.
  TabStripWindowTracker new_otr(
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_FALSE(new_otr.had_existing_window());
}

}  // namespace
}  // namespace android
