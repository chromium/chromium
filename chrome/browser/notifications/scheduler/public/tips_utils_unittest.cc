// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/tips_utils.h"

#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace notifications {
namespace {

TEST(NotificationTipsUtilsTest, GetTipsNotificationData) {
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
