// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/utils.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"

namespace companion {

std::string GetHomepageURLForCompanion() {
  std::string url = base::GetFieldTrialParamValueByFeature(
      features::kSidePanelCompanion, "companion-homepage-url");
  if (url.empty()) {
    return std::string("https://lens.google.com/companion");
  }
  return url;
}

std::string GetImageUploadURLForCompanion() {
  std::string url = base::GetFieldTrialParamValueByFeature(
      features::kSidePanelCompanion, "companion-image-upload-url");
  if (url.empty()) {
    return std::string("https://lens.google.com/upload");
  }
  return url;
}

bool ShouldEnableOpenCompanionForImageSearch() {
  if (base::FeatureList::IsEnabled(features::kSidePanelCompanion)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::kSidePanelCompanion, "open-companion-for-image-search", true);
  }
  return false;
}

bool ShouldEnableOpenCompanionForWebSearch() {
  if (base::FeatureList::IsEnabled(features::kSidePanelCompanion)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::kSidePanelCompanion, "open-companion-for-web-search", true);
  }
  return false;
}
bool ShouldOpenLinksInCurrentTab() {
  if (base::FeatureList::IsEnabled(features::kSidePanelCompanion)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::kSidePanelCompanion, "open-links-in-current-tab", true);
  }
  return false;
}

std::string GetExpsRegistrationSuccessPageURLs() {
  return base::GetFieldTrialParamValueByFeature(
      features::kCompanionEnabledByObservingExpsNavigations,
      "exps-registration-success-page-urls");
}

}  // namespace companion
