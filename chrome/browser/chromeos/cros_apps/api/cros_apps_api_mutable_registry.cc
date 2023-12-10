// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_mutable_registry.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_infos.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

const void* kUserDataKey = &kUserDataKey;

// static
CrosAppsApiMutableRegistry& CrosAppsApiMutableRegistry::GetInstance(
    Profile* profile) {
  if (!profile->GetUserData(&kUserDataKey)) {
    profile->SetUserData(
        kUserDataKey, std::make_unique<CrosAppsApiMutableRegistry>(PassKey()));
  }

  return *static_cast<CrosAppsApiMutableRegistry*>(
      profile->GetUserData(&kUserDataKey));
}

CrosAppsApiMutableRegistry::~CrosAppsApiMutableRegistry() = default;

CrosAppsApiMutableRegistry::CrosAppsApiMutableRegistry(PassKey)
    : apis_(CreateDefaultCrosAppsApiInfo()) {}

bool CrosAppsApiMutableRegistry::IsApiEnabledFor(
    const CrosAppsApiInfo& api_info,
    content::NavigationHandle* navigation_handle) const {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    // Only main frames can have access to APIs.
    return false;
  }

  const auto& url = navigation_handle->GetURL();

  if (!IsUrlEligibleForCrosAppsApis(url)) {
    return false;
  }

  // TODO(b/311528206): Decide if this scheme check should be removed.
  //
  // The following schemes because they share the same origin as their creator
  // (i.e. the App), and could cause problem during origin matching.
  //
  // The app could inadvertently create these URLs that serves third-party (from
  // the App's perspective) untrustworthy content. Said third-party content
  // probably shouldn't be treated as same origin as the app.
  if (url.SchemeIs(url::kBlobScheme) || url.SchemeIs(url::kFileSystemScheme)) {
    return false;
  }

  const bool are_required_features_enabled = base::ranges::all_of(
      api_info.required_features(), [](const auto& base_feature) {
        return base::FeatureList::IsEnabled(base_feature);
      });

  if (!are_required_features_enabled) {
    return false;
  }

  const bool is_allowlisted_origin = base::ranges::any_of(
      api_info.allowed_origins(),
      [&url](const auto& origin) { return origin.IsSameOriginWith(url); });

  if (!is_allowlisted_origin) {
    return false;
  }

  return true;
}

std::vector<CrosAppsApiInfo::EnableBlinkRuntimeFeatureFunction>
CrosAppsApiMutableRegistry::GetBlinkFeatureEnablementFunctionsFor(
    content::NavigationHandle* navigation_handle) const {
  std::vector<CrosAppsApiInfo::EnableBlinkRuntimeFeatureFunction> fns;
  fns.reserve(apis_.size());

  for (const auto& [_, api] : apis_) {
    if (IsApiEnabledFor(api, navigation_handle)) {
      fns.push_back(api.enable_blink_runtime_feature_fn());
    }
  }

  return fns;
}

void CrosAppsApiMutableRegistry::AddOrReplaceForTesting(
    CrosAppsApiInfo api_info) {
  CHECK_IS_TEST();

  auto key = api_info.blink_feature();
  apis_.insert_or_assign(key, std::move(api_info));
}
