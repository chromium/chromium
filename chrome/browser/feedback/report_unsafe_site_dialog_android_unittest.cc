// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site_dialog.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

class ReportUnsafeSiteDialogAndroidTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ReportUnsafeSiteDialogAndroidTest, IsNotEnabled) {
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

}  // namespace feedback
