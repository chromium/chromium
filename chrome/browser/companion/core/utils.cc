// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/utils.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"

namespace companion {

namespace {
const base::Feature* GetFeatureToUse() {
  if (base::FeatureList::IsEnabled(features::internal::kSidePanelCompanion)) {
    return &features::internal::kSidePanelCompanion;
  }

  if (base::FeatureList::IsEnabled(features::internal::kSidePanelCompanion2)) {
    return &features::internal::kSidePanelCompanion2;
  }

  if (base::FeatureList::IsEnabled(
          features::internal::kCompanionEnabledByObservingExpsNavigations)) {
    return &features::internal::kCompanionEnabledByObservingExpsNavigations;
  }
  NOTREACHED();
  return &features::internal::kSidePanelCompanion;
}
}  // namespace

std::string GetHomepageURLForCompanion() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  std::string url = base::GetFieldTrialParamValueByFeature(
      *GetFeatureToUse(), "companion-homepage-url");
  if (url.empty()) {
    return std::string("https://lens.google.com/companion");
  }
  return url;
}

std::string GetImageUploadURLForCompanion() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  std::string url = base::GetFieldTrialParamValueByFeature(
      *GetFeatureToUse(), "companion-image-upload-url");
  if (url.empty()) {
    return std::string("https://lens.google.com/upload");
  }
  return url;
}

bool ShouldEnableOpenCompanionForImageSearch() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "open-companion-for-image-search", true);
}

bool ShouldEnableOpenCompanionForWebSearch() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.

  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "open-companion-for-web-search", true);
}
bool ShouldOpenLinksInCurrentTab() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "open-links-in-current-tab", true);
}

std::string GetExpsRegistrationSuccessPageURLs() {
  return base::GetFieldTrialParamValueByFeature(
      features::internal::kCompanionEnabledByObservingExpsNavigations,
      "exps-registration-success-page-urls");
}

}  // namespace companion
