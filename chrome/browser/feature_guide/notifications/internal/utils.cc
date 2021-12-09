// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/utils.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_list.h"

namespace feature_guide {
namespace {

constexpr char kCustomDataKeyForFeatureType[] = "feature_type";

constexpr char kNotificationIdDefaultBroser[] = "feature_guide_default_browser";
constexpr char kNotificationIdSignIn[] = "feature_guide_sign_in";
constexpr char kNotificationIdIncognitoTab[] = "feature_guide_incognito_tab";
constexpr char kNotificationIdVoiceSearch[] = "feature_guide_voice_search";
constexpr char kNotificationIdNTPSuggestionCard[] =
    "feature_guide_ntp_suggestion_card";

}  // namespace

void FeatureToCustomData(FeatureType feature,
                         std::map<std::string, std::string>* custom_data) {
  custom_data->emplace(kCustomDataKeyForFeatureType,
                       base::NumberToString(static_cast<int>(feature)));
}

FeatureType FeatureFromCustomData(
    const std::map<std::string, std::string>& custom_data) {
  int parsed_value = 0;
  std::string feature_string = custom_data.at(kCustomDataKeyForFeatureType);
  if (!base::StringToInt(feature_string, &parsed_value))
    return FeatureType::kInvalid;

  return static_cast<FeatureType>(parsed_value);
}

std::string NotificationIdForFeature(FeatureType feature) {
  switch (feature) {
    case FeatureType::kDefaultBrowser:
      return kNotificationIdDefaultBroser;
    case FeatureType::kSignIn:
      return kNotificationIdSignIn;
    case FeatureType::kIncognitoTab:
      return kNotificationIdIncognitoTab;
    case FeatureType::kNTPSuggestionCard:
      return kNotificationIdNTPSuggestionCard;
    case FeatureType::kVoiceSearch:
      return kNotificationIdVoiceSearch;
    default:
      NOTREACHED();
      return std::string();
  }
}

FeatureType NotificationIdToFeature(const std::string& notification_id) {
  if (notification_id == kNotificationIdDefaultBroser) {
    return FeatureType::kDefaultBrowser;
  } else if (notification_id == kNotificationIdSignIn) {
    return FeatureType::kSignIn;
  } else if (notification_id == kNotificationIdIncognitoTab) {
    return FeatureType::kIncognitoTab;
  } else if (notification_id == kNotificationIdNTPSuggestionCard) {
    return FeatureType::kNTPSuggestionCard;
  } else if (notification_id == kNotificationIdVoiceSearch) {
    return FeatureType::kVoiceSearch;
  }
  return FeatureType::kInvalid;
}

#if defined(OS_ANDROID)
base::Feature GetNotificationIphFeatureForFeature(FeatureType& feature) {
  switch (feature) {
    case FeatureType::kIncognitoTab:
      return feature_engagement::
          kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature;
    case FeatureType::kNTPSuggestionCard:
      return feature_engagement::
          kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature;
    case FeatureType::kVoiceSearch:
      return feature_engagement::
          kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature;
    case FeatureType::kDefaultBrowser:
      return feature_engagement::
          kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature;
    case FeatureType::kSignIn:
      return feature_engagement::
          kIPHFeatureNotificationGuideSignInNotificationShownFeature;
    default:
      NOTREACHED();
      return feature_engagement::
          kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature;
  }
}
#endif

}  // namespace feature_guide
