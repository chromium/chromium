// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/justifications.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list::test {

class JustificationsTest : public ::testing::Test {
 public:
  JustificationsTest() {
    scoped_features_.InitWithFeatures(
        {}, {ash::features::kLauncherContinueSectionWithRecents,
             ash::features::kLauncherContinueSectionWithRecentsRollout});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(JustificationsTest, OpenedOrEdited) {
  base::Time now = base::Time::Now();

  // Opened more recently than edited.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kViewed,
                             now - base::Seconds(10), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_JUST_NOW));

  // Edited more recently than opened.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kModified,
                             now - base::Seconds(10), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));
}

TEST_F(JustificationsTest, EditTimes) {
  base::Time now = base::Time::Now();

  // Just now.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kModified,
                             now - base::Minutes(5), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));

  // Today.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kModified,
                             now - base::Hours(23), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_TODAY));

  // Yesterday.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kModified,
                             now - base::Hours(47), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_YESTERDAY));

  // Past week.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kModified,
                             now - base::Days(6), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_PAST_WEEK));

  // Past month.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kModified,
                             now - base::Days(30), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_PAST_MONTH));

  // No string, file too old.
  EXPECT_FALSE(
      GetJustificationString(ash::FileSuggestionJustificationType::kModified,
                             now - base::Days(32), /*user_name=*/""));
}

TEST_F(JustificationsTest, OpenTimes) {
  base::Time now = base::Time::Now();

  // Just now.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kViewed,
                             now - base::Minutes(5), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_JUST_NOW));

  // Today.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kViewed,
                             now - base::Hours(23), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_TODAY));

  // Yesterday.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kViewed,
                             now - base::Hours(47), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_YESTERDAY));

  // Past week.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kViewed,
                             now - base::Days(6), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_PAST_WEEK));

  // Past month.
  EXPECT_EQ(
      GetJustificationString(ash::FileSuggestionJustificationType::kViewed,
                             now - base::Days(30), /*user_name=*/""),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_PAST_MONTH));
}

}  // namespace app_list::test
