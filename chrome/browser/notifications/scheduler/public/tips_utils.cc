// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/tips_utils.h"

#include <map>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace notifications {
namespace {

const std::map<TipsNotificationsFeatureType, std::pair<int, int>>&
GetTipsNotificationsFeatureTypeMap() {
  static const base::NoDestructor<
      std::map<TipsNotificationsFeatureType, std::pair<int, int>>>
      kTipsNotificationsFeatureTypeMap({
          {TipsNotificationsFeatureType::kEnhancedSafeBrowsing,
           {IDS_TIPS_NOTIFICATIONS_ENHANCED_SAFE_BROWSING_TITLE,
            IDS_TIPS_NOTIFICATIONS_ENHANCED_SAFE_BROWSING_SUBTITLE}},
          {TipsNotificationsFeatureType::kQuickDelete,
           {IDS_TIPS_NOTIFICATIONS_QUICK_DELETE_TITLE,
            IDS_TIPS_NOTIFICATIONS_QUICK_DELETE_SUBTITLE}},
          {TipsNotificationsFeatureType::kGoogleLens,
           {IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_TITLE,
            IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_SUBTITLE}},
          {TipsNotificationsFeatureType::kBottomOmnibox,
           {IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_TITLE,
            IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_SUBTITLE}},
      });
  return *kTipsNotificationsFeatureTypeMap;
}

}  // namespace

NotificationData GetTipsNotificationData(
    TipsNotificationsFeatureType feature_type) {
  const auto& map = GetTipsNotificationsFeatureTypeMap();
  const auto it = map.find(feature_type);
  DCHECK(it != map.end());

  NotificationData data;
  data.title = l10n_util::GetStringUTF16(it->second.first);
  data.message = l10n_util::GetStringUTF16(it->second.second);
  data.custom_data[kTipsNotificationsFeatureType] =
      base::NumberToString(static_cast<int>(feature_type));
  data.buttons.clear();
  NotificationData::Button open_chrome_button;
  open_chrome_button.type = ActionButtonType::kHelpful;
  open_chrome_button.id = kDefaultHelpfulButtonId;
  open_chrome_button.text =
      l10n_util::GetStringUTF16(IDS_TIPS_NOTIFICATIONS_HELPFUL_BUTTON_TEXT);
  data.buttons.emplace_back(std::move(open_chrome_button));
  return data;
}

#if BUILDFLAG(IS_ANDROID)
std::string GetFeatureTypePref(TipsNotificationsFeatureType feature_type) {
  switch (feature_type) {
    case TipsNotificationsFeatureType::kEnhancedSafeBrowsing:
      return prefs::kAndroidTipNotificationShownESB;
    case TipsNotificationsFeatureType::kQuickDelete:
      return prefs::kAndroidTipNotificationShownQuickDelete;
    case TipsNotificationsFeatureType::kGoogleLens:
      return prefs::kAndroidTipNotificationShownLens;
    case TipsNotificationsFeatureType::kBottomOmnibox:
      return prefs::kAndroidTipNotificationShownBottomOmnibox;
    default:
      NOTREACHED();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace notifications
