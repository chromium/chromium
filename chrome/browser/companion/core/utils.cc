// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/utils.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "net/base/url_util.h"

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
  NOTREACHED_IN_MIGRATION();
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
    return std::string("https://lens.google.com/v3/upload");
  }
  return url;
}

bool GetShouldIssuePreconnectForCompanion() {
  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "companion-issue-preconnect", true);
}

std::string GetPreconnectKeyForCompanion() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  std::string url = base::GetFieldTrialParamValueByFeature(
      *GetFeatureToUse(), "companion-preconnect-key");
  if (url.empty()) {
    return std::string("chrome-untrusted://companion-side-panel.top-chrome");
  }
  return url;
}

bool GetShouldIssueProcessPrewarmingForCompanion() {
  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "companion-issue-process-prewarming", true);
}

bool ShouldEnableOpenCompanionForImageSearch() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "open-companion-for-image-search", false);
}

bool ShouldEnableOpenCompanionForWebSearch() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.

  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "open-companion-for-web-search", false);
}
bool ShouldOpenLinksInCurrentTab() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "open-links-in-current-tab", false);
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

bool ShouldOpenContextualLensPanel() {
  // Allow multiple field trials to control the value. This is needed because
  // companion may be enabled by any of the field trials.
  return base::GetFieldTrialParamByFeatureAsBool(
      *GetFeatureToUse(), "open-contextual-lens-panel", false);
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

// Checks to see if the page url is safe to open in Chrome.
bool IsSafeURLFromCompanion(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  static constexpr auto chrome_domain_allowlists =
      base::MakeFixedFlatSet<std::string_view>({"chrome://settings/syncSetup"});
  std::string_view url_string(url.spec());

  if (!url.SchemeIsHTTPOrHTTPS() &&
      !chrome_domain_allowlists.contains(url_string)) {
    return false;
  }

  return true;
}

}  // namespace companion
