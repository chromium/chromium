// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/core/tips_utils.h"

#include <map>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace tips {
namespace {

// TODO(crbug.com/559296862): Deprecate GetTipsNotificationsFeatureTypeMap after
// tips self service is launched and the legacy segmentation flow is removed.
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
          {TipsNotificationsFeatureType::kPasswordAutofill,
           {IDS_TIPS_NOTIFICATIONS_PASSWORD_AUTOFILL_TITLE,
            IDS_TIPS_NOTIFICATIONS_PASSWORD_AUTOFILL_SUBTITLE}},
          {TipsNotificationsFeatureType::kSignin,
           {IDS_TIPS_NOTIFICATIONS_SIGNIN_TITLE,
            IDS_TIPS_NOTIFICATIONS_SIGNIN_SUBTITLE}},
          {TipsNotificationsFeatureType::kCreateTabGroups,
           {IDS_TIPS_NOTIFICATIONS_CREATE_TAB_GROUPS_TITLE,
            IDS_TIPS_NOTIFICATIONS_CREATE_TAB_GROUPS_SUBTITLE}},
          {TipsNotificationsFeatureType::kCustomizeMVT,
           {IDS_TIPS_NOTIFICATIONS_CUSTOMIZE_MVT_TITLE,
            IDS_TIPS_NOTIFICATIONS_CUSTOMIZE_MVT_SUBTITLE}},
          {TipsNotificationsFeatureType::kRecentTabs,
           {IDS_TIPS_NOTIFICATIONS_RECENT_TABS_TITLE,
            IDS_TIPS_NOTIFICATIONS_RECENT_TABS_SUBTITLE}},
      });
  return *kTipsNotificationsFeatureTypeMap;
}

}  // namespace

notifications::NotificationData CreateTipsNotificationData(
    TipsNotificationsFeatureType feature_type,
    int title_id,
    int subtitle_id) {
  notifications::NotificationData data;
  data.title = l10n_util::GetStringUTF16(title_id);
  data.message = l10n_util::GetStringUTF16(subtitle_id);
  data.custom_data[notifications::kTipsNotificationsFeatureType] =
      base::NumberToString(static_cast<int>(feature_type));
  data.buttons.clear();
  notifications::NotificationData::Button open_chrome_button;
  open_chrome_button.type = notifications::ActionButtonType::kHelpful;
  open_chrome_button.id = notifications::kDefaultHelpfulButtonId;
  open_chrome_button.text =
      l10n_util::GetStringUTF16(IDS_TIPS_NOTIFICATIONS_HELPFUL_BUTTON_TEXT);
  data.buttons.emplace_back(std::move(open_chrome_button));
  return data;
}

// TODO(crbug.com/559296862): Deprecate GetTipsNotificationData after tips self
// service is launched and the legacy segmentation flow is removed.
notifications::NotificationData GetTipsNotificationData(
    TipsNotificationsFeatureType feature_type) {
  const auto& map = GetTipsNotificationsFeatureTypeMap();
  const auto it = map.find(feature_type);
  DCHECK(it != map.end());
  return CreateTipsNotificationData(feature_type, it->second.first,
                                    it->second.second);
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
    case TipsNotificationsFeatureType::kPasswordAutofill:
      return prefs::kAndroidTipNotificationShownPasswordAutofill;
    case TipsNotificationsFeatureType::kSignin:
      return prefs::kAndroidTipNotificationShownSignin;
    case TipsNotificationsFeatureType::kCreateTabGroups:
      return prefs::kAndroidTipNotificationShownCreateTabGroups;
    case TipsNotificationsFeatureType::kCustomizeMVT:
      return prefs::kAndroidTipNotificationShownCustomizeMVT;
    case TipsNotificationsFeatureType::kRecentTabs:
      return prefs::kAndroidTipNotificationShownRecentTabs;
    default:
      NOTREACHED();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace tips
