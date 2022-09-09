// Copyright 2021 The Chromium Authors
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

#if BUILDFLAG(IS_ANDROID)
const base::Feature& GetNotificationIphFeatureForFeature(FeatureType& feature) {
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

const base::Feature* GetUsedIphFeatureForFeature(FeatureType& feature) {
  switch (feature) {
    case FeatureType::kIncognitoTab:
      return &feature_engagement::
          kIPHFeatureNotificationGuideIncognitoTabUsedFeature;
    case FeatureType::kVoiceSearch:
      return &feature_engagement::
          kIPHFeatureNotificationGuideVoiceSearchUsedFeature;
    case FeatureType::kNTPSuggestionCard:
    case FeatureType::kDefaultBrowser:
    case FeatureType::kSignIn:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}
#endif

bool ShouldTargetLowEngagedUsers(FeatureType feature) {
  switch (feature) {
    case FeatureType::kIncognitoTab:
    case FeatureType::kVoiceSearch:
    case FeatureType::kNTPSuggestionCard:
      return true;
    case FeatureType::kDefaultBrowser:
    case FeatureType::kSignIn:
    default:
      return false;
  }
}

}  // namespace feature_guide
