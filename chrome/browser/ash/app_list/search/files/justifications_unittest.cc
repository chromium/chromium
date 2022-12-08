// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/justifications.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list::test {

TEST(JustificationsTest, OpenedOrEdited) {
  base::Time now = base::Time::Now();

  // Opened more recently than edited.
  EXPECT_EQ(
      GetJustificationString(now - base::Seconds(10), now - base::Seconds(30)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_JUST_NOW));

  // Edited more recently than opened.
  EXPECT_EQ(
      GetJustificationString(now - base::Seconds(30), now - base::Seconds(10)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));

  // When tied, should be edited.
  EXPECT_EQ(
      GetJustificationString(now - base::Seconds(30), now - base::Seconds(30)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));
}

TEST(JustificationsTest, EditTimes) {
  base::Time now = base::Time::Now();

  // Just now.
  EXPECT_EQ(
      GetJustificationString(now - base::Minutes(5), now - base::Minutes(5)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW));

  // Today.
  EXPECT_EQ(
      GetJustificationString(now - base::Hours(23), now - base::Hours(23)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_TODAY));

  // Yesterday.
  EXPECT_EQ(
      GetJustificationString(now - base::Hours(47), now - base::Hours(47)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_YESTERDAY));

  // Past week.
  EXPECT_EQ(GetJustificationString(now - base::Days(6), now - base::Days(6)),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_PAST_WEEK));

  // Past month.
  EXPECT_EQ(GetJustificationString(now - base::Days(30), now - base::Days(30)),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_PAST_MONTH));

  // No string, file too old.
  EXPECT_FALSE(
      GetJustificationString(now - base::Days(32), now - base::Days(32)));
}

TEST(JustificationsTest, OpenTimes) {
  base::Time now = base::Time::Now();

  // Just now.
  EXPECT_EQ(
      GetJustificationString(now - base::Minutes(5), now - base::Days(100)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_JUST_NOW));

  // Today.
  EXPECT_EQ(
      GetJustificationString(now - base::Hours(23), now - base::Days(100)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_TODAY));

  // Yesterday.
  EXPECT_EQ(
      GetJustificationString(now - base::Hours(47), now - base::Days(100)),
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_YESTERDAY));

  // Past week.
  EXPECT_EQ(GetJustificationString(now - base::Days(6), now - base::Days(100)),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_PAST_WEEK));

  // Past month.
  EXPECT_EQ(GetJustificationString(now - base::Days(30), now - base::Days(100)),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_PAST_MONTH));
}

}  // namespace app_list::test
