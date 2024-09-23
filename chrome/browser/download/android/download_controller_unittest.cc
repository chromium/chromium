// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_controller.h"

#include "base/json/values_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;
using ::testing::Return;

class DownloadControllerTest : public testing::Test {
 public:
  DownloadControllerTest() {
    content::DownloadItemUtils::AttachInfoForTesting(&item_, &profile_,
                                                     nullptr);
  }

  DownloadControllerTest(const DownloadControllerTest&) = delete;
  DownloadControllerTest& operator=(const DownloadControllerTest&) = delete;

  download::MockDownloadItem* item() { return &item_; }
  Profile* profile() { return &profile_; }

  bool ShouldShowAppVerificationPrompt(download::DownloadItem* item) {
    return controller_.ShouldShowAppVerificationPrompt(item);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  DownloadController controller_;
  download::MockDownloadItem item_;
};

TEST_F(DownloadControllerTest, ShouldShowAppVerificationPrompt) {
  EXPECT_FALSE(ShouldShowAppVerificationPrompt(item()));

  EXPECT_CALL(*item(), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  EXPECT_TRUE(ShouldShowAppVerificationPrompt(item()));

  {
    ScopedListPrefUpdate update(
        profile()->GetPrefs(), prefs::kDownloadAppVerificationPromptTimestamps);
    update->Append(base::TimeToValue(base::Time::Now()));
    update->Append(base::TimeToValue(base::Time::Now()));
    update->Append(base::TimeToValue(base::Time::Now()));
  }

  EXPECT_FALSE(ShouldShowAppVerificationPrompt(item()));
}

TEST_F(DownloadControllerTest, CleanupOldTimestamps) {
  EXPECT_CALL(*item(), GetDangerType())
      .WillOnce(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));

  {
    ScopedListPrefUpdate update(
        profile()->GetPrefs(), prefs::kDownloadAppVerificationPromptTimestamps);
    base::Time long_ago = base::Time::Now() - base::Days(500);
    update->Append(base::TimeToValue(long_ago));
    update->Append(base::TimeToValue(long_ago));
    update->Append(base::TimeToValue(long_ago));
  }

  EXPECT_TRUE(ShouldShowAppVerificationPrompt(item()));

  EXPECT_THAT(profile()->GetPrefs()->GetList(
                  prefs::kDownloadAppVerificationPromptTimestamps),
              IsEmpty());
}
