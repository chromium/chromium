// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/clipboard_toast_tracker.h"

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_data_protection {

class ClipboardToastTrackerTest : public testing::Test {
 public:
  ClipboardToastTrackerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(ClipboardToastTrackerTest, GetForProfile) {
  auto* tracker1 = ClipboardToastTracker::GetForProfile(profile_);
  ASSERT_NE(tracker1, nullptr);

  auto* tracker2 = ClipboardToastTracker::GetForProfile(profile_);
  EXPECT_EQ(tracker1, tracker2);

  EXPECT_EQ(ClipboardToastTracker::GetForProfile(nullptr), nullptr);
}

TEST_F(ClipboardToastTrackerTest, ShouldShowToastInitially) {
  auto* tracker = ClipboardToastTracker::GetForProfile(profile_);
  ASSERT_NE(tracker, nullptr);

  EXPECT_TRUE(tracker->ShouldShowToast(CopyToastType::kAudit));
  EXPECT_TRUE(tracker->ShouldShowToast(CopyToastType::kKeptInManagedChrome));
}

TEST_F(ClipboardToastTrackerTest, RecordToastShown) {
  auto* tracker = ClipboardToastTracker::GetForProfile(profile_);
  ASSERT_NE(tracker, nullptr);

  tracker->RecordToastShown(CopyToastType::kAudit);
  EXPECT_FALSE(tracker->ShouldShowToast(CopyToastType::kAudit));
  EXPECT_TRUE(tracker->ShouldShowToast(CopyToastType::kKeptInManagedChrome));

  tracker->RecordToastShown(CopyToastType::kKeptInManagedChrome);
  EXPECT_FALSE(tracker->ShouldShowToast(CopyToastType::kAudit));
  EXPECT_FALSE(tracker->ShouldShowToast(CopyToastType::kKeptInManagedChrome));
}

}  // namespace enterprise_data_protection
