// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/utils.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "net/base/url_util.h"

namespace companion {

std::string GetHomepageURLForCompanion() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  std::string url = base::GetFieldTrialParamValueByFeature(
      features::internal::kSidePanelCompanion, "companion-homepage-url");

  if (url.empty()) {
    url = base::GetFieldTrialParamValueByFeature(
        features::internal::kCompanionEnabledByObservingExpsNavigations,
        "companion-homepage-url");
  }
  if (url.empty()) {
    return std::string("https://lens.google.com/companion");
  }
  return url;
}

std::string GetImageUploadURLForCompanion() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  std::string url = base::GetFieldTrialParamValueByFeature(
      features::internal::kSidePanelCompanion, "companion-image-upload-url");
  if (url.empty()) {
    url = base::GetFieldTrialParamValueByFeature(
        features::internal::kCompanionEnabledByObservingExpsNavigations,
        "companion-image-upload-url");
  }
  if (url.empty()) {
    return std::string("https://lens.google.com/upload");
  }
  return url;
}

bool ShouldEnableOpenCompanionForImageSearch() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  if (base::FeatureList::IsEnabled(features::internal::kSidePanelCompanion)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::internal::kSidePanelCompanion,
        "open-companion-for-image-search", true);
  }

  if (base::FeatureList::IsEnabled(
          features::internal::kCompanionEnabledByObservingExpsNavigations)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::internal::kCompanionEnabledByObservingExpsNavigations,
        "open-companion-for-image-search", true);
  }
  NOTREACHED();
  return false;
}

bool ShouldEnableOpenCompanionForWebSearch() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  if (base::FeatureList::IsEnabled(features::internal::kSidePanelCompanion)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::internal::kSidePanelCompanion,
        "open-companion-for-web-search", true);
  }

  if (base::FeatureList::IsEnabled(
          features::internal::kCompanionEnabledByObservingExpsNavigations)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::internal::kCompanionEnabledByObservingExpsNavigations,
        "open-companion-for-web-search", true);
  }
  NOTREACHED();
  return false;
}
bool ShouldOpenLinksInCurrentTab() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  if (base::FeatureList::IsEnabled(features::internal::kSidePanelCompanion)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::internal::kSidePanelCompanion, "open-links-in-current-tab",
        true);
  }

  if (base::FeatureList::IsEnabled(
          features::internal::kCompanionEnabledByObservingExpsNavigations)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::internal::kCompanionEnabledByObservingExpsNavigations,
        "open-links-in-current-tab", true);
  }
  NOTREACHED();
  return false;
}

std::string GetExpsRegistrationSuccessPageURLs() {
  return base::GetFieldTrialParamValueByFeature(
      features::internal::kCompanionEnabledByObservingExpsNavigations,
      "exps-registration-success-page-urls");
}

std::string GetCompanionIPHBlocklistedPageURLs() {
  return base::GetFieldTrialParamValueByFeature(
      features::internal::kCompanionEnabledByObservingExpsNavigations,
      "companion-iph-blocklisted-page-urls");
}

// Checks to see if the page url is a valid one to be sent to companion.
bool IsValidPageURLForCompanion(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  if (!url.has_host()) {
    return false;
  }
  if (net::IsLocalhost(url)) {
    return false;
  }
  if (url.HostIsIPAddress()) {
    return false;
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (url.has_username() || url.has_password()) {
    return false;
  }
  return true;
}

}  // namespace companion
