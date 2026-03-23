// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_mutable_registry.h"

#include <algorithm>
#include <memory>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_infos.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_utils.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

const void* kUserDataKey = &kUserDataKey;

// static
CrosAppsApiMutableRegistry& CrosAppsApiMutableRegistry::GetInstance(
    content::BrowserContext* context) {
  if (!context->GetUserData(&kUserDataKey)) {
    context->SetUserData(
        kUserDataKey,
        std::make_unique<CrosAppsApiMutableRegistry>(PassKey(), context));
  }

  return *static_cast<CrosAppsApiMutableRegistry*>(
      context->GetUserData(&kUserDataKey));
}

CrosAppsApiMutableRegistry::~CrosAppsApiMutableRegistry() = default;

CrosAppsApiMutableRegistry::CrosAppsApiMutableRegistry(
    PassKey,
    content::BrowserContext* context)
    : context_(context), api_infos_(CreateDefaultCrosAppsApiInfo()) {}

bool CrosAppsApiMutableRegistry::CanEnableApi(
    const CrosAppsApiId api_id) const {
  const auto iter = api_infos_.find(api_id);
  CHECK(iter != api_infos_.end());
  return CanEnableApi(iter->second);
}

bool CrosAppsApiMutableRegistry::CanEnableApi(
    const CrosAppsApiInfo& api_info) const {
  const bool are_required_features_enabled = std::ranges::all_of(
      api_info.required_features(), [](const auto& base_feature) {
        return base::FeatureList::IsEnabled(base_feature);
      });

  if (!are_required_features_enabled) {
    return false;
  }

  return true;
}

bool CrosAppsApiMutableRegistry::IsApiEnabledForFrame(
    const CrosAppsApiInfo& api_info,
    const CrosAppsApiFrameContext& api_context) const {
  // The API enablement check must be performed on the BrowserContext which
  // `this` registry was created for. See CrosAppsApiRegistry::GetInstance().
  CHECK_EQ(context_, api_context.GetBrowserContext());

  if (!CanEnableApi(api_info)) {
    return false;
  }

  const auto& url = api_context.GetUrl();

  if (!IsUrlEligibleForCrosAppsApis(url)) {
    return false;
  }

  if (!api_context.IsPrimaryMainFrame()) {
    // Only primary main frames can access the APIs.
    return false;
  }

  const bool is_allowlisted_origin = std::ranges::any_of(
      api_info.allowed_origins(),
      [&url](const auto& origin) { return origin.IsSameOriginWith(url); });

  if (!is_allowlisted_origin) {
    return false;
  }

  return true;
}

std::vector<CrosAppsApiInfo::EnableBlinkRuntimeFeatureFunction>
CrosAppsApiMutableRegistry::GetBlinkFeatureEnablementFunctionsForFrame(
    const CrosAppsApiFrameContext& api_context) const {
  std::vector<CrosAppsApiInfo::EnableBlinkRuntimeFeatureFunction> fns;
  fns.reserve(api_infos_.size());

  for (const auto& [_, api_info] : api_infos_) {
    if (IsApiEnabledForFrame(api_info, api_context)) {
      fns.push_back(api_info.enable_blink_runtime_feature_fn());
    }
  }

  return fns;
}

bool CrosAppsApiMutableRegistry::IsApiEnabledForFrame(
    const CrosAppsApiId api_id,
    const CrosAppsApiFrameContext& api_context) const {
  const auto iter = api_infos_.find(api_id);
  CHECK(iter != api_infos_.end());
  return IsApiEnabledForFrame(iter->second, api_context);
}

void CrosAppsApiMutableRegistry::AddOrReplaceForTesting(
    CrosAppsApiInfo api_info) {
  CHECK_IS_TEST();

  auto key = api_info.api_id();
  api_infos_.insert_or_assign(key, std::move(api_info));
}
