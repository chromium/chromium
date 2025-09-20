// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/schedule_service_utils.h"

#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace notifications {
namespace {

struct TestCaseInput {
  // A list of fake current time inputs.
  base::Time fake_now;

  // Expected output data.
  TimePair expected;

  TestCaseInput(std::string fake_now_str,
                std::string expected_window_start_str,
                std::string expected_window_end_str) {
    EXPECT_TRUE(base::Time::FromString(fake_now_str.c_str(), &this->fake_now));
    EXPECT_TRUE(base::Time::FromString(expected_window_start_str.c_str(),
                                       &this->expected.first));
    EXPECT_TRUE(base::Time::FromString(expected_window_end_str.c_str(),
                                       &this->expected.second));
  }
};

// Assume fixed suggested time window, and try different faked current time to
// verify next actual deliver time window calculation.
TEST(NotificationScheduleServiceUtilsTest, NextTimeWindow) {
  // Build test cases.
  TimeDeltaPair morning_window_input = {base::Hours(5), base::Hours(7)};
  TimeDeltaPair evening_window_input = {base::Hours(18), base::Hours(20)};

  std::vector<TestCaseInput> test_cases = {
      {"03/24/19 04:05:55 AM", "03/24/19 05:00:00 AM", "03/24/19 07:00:00 AM"},
      {"03/24/19 11:45:22 AM", "03/24/19 06:00:00 PM", "03/24/19 08:00:00 PM"},
      {"03/24/19 11:45:22 PM", "03/25/19 05:00:00 AM", "03/25/19 07:00:00 AM"},
  };

  // Run test cases.
  for (size_t i = 0; i < test_cases.size(); i++) {
    notifications::test::FakeClock clock;
    clock.SetNow(test_cases[i].fake_now);
    TimePair actual_output;
    EXPECT_TRUE(NextTimeWindow(&clock, morning_window_input,
                               evening_window_input, &actual_output));
    EXPECT_EQ(actual_output, test_cases[i].expected);
  }
}

TEST(NotificationScheduleServiceUtilsTest, GetTipsNotificationData) {
  const std::vector<TipsNotificationsFeatureType> tips_list = {
      TipsNotificationsFeatureType::kEnhancedSafeBrowsing,
      TipsNotificationsFeatureType::kQuickDelete,
      TipsNotificationsFeatureType::kGoogleLens,
      TipsNotificationsFeatureType::kBottomOmnibox};

  for (const auto type : tips_list) {
    NotificationData data = GetTipsNotificationData(type);
    std::u16string expected_title;
    std::u16string expected_message;

    switch (type) {
      case TipsNotificationsFeatureType::kEnhancedSafeBrowsing:
        expected_title = l10n_util::GetStringUTF16(
            IDS_TIPS_NOTIFICATIONS_ENHANCED_SAFE_BROWSING_TITLE);
        expected_message = l10n_util::GetStringUTF16(
            IDS_TIPS_NOTIFICATIONS_ENHANCED_SAFE_BROWSING_SUBTITLE);
        break;
      case TipsNotificationsFeatureType::kQuickDelete:
        expected_title = l10n_util::GetStringUTF16(
            IDS_TIPS_NOTIFICATIONS_QUICK_DELETE_TITLE);
        expected_message = l10n_util::GetStringUTF16(
            IDS_TIPS_NOTIFICATIONS_QUICK_DELETE_SUBTITLE);
        break;
      case TipsNotificationsFeatureType::kGoogleLens:
        expected_title =
            l10n_util::GetStringUTF16(IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_TITLE);
        expected_message = l10n_util::GetStringUTF16(
            IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_SUBTITLE);
        break;
      case TipsNotificationsFeatureType::kBottomOmnibox:
        expected_title = l10n_util::GetStringUTF16(
            IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_TITLE);
        expected_message = l10n_util::GetStringUTF16(
            IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_SUBTITLE);
        break;
      default:
        NOTREACHED();
    }

    EXPECT_EQ(data.title, expected_title);
    EXPECT_EQ(data.message, expected_message);
    EXPECT_EQ(data.custom_data[kTipsNotificationsFeatureType],
              base::NumberToString(static_cast<int>(type)));

    EXPECT_EQ(data.buttons.size(), 1u);
    EXPECT_EQ(
        data.buttons[0].text,
        l10n_util::GetStringUTF16(IDS_TIPS_NOTIFICATIONS_HELPFUL_BUTTON_TEXT));
  }
}

}  // namespace

}  // namespace notifications
