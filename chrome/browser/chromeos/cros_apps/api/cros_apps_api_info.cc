// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

CrosAppsApiInfo::CrosAppsApiInfo(CrosAppsApiId api_id,
                                 EnableBlinkRuntimeFeatureFunction enable_fn)
    : api_id_(api_id), enable_blink_runtime_feature_fn_(enable_fn) {}

CrosAppsApiInfo::CrosAppsApiInfo(CrosAppsApiInfo&&) = default;

CrosAppsApiInfo& CrosAppsApiInfo::operator=(CrosAppsApiInfo&&) = default;

CrosAppsApiInfo::~CrosAppsApiInfo() = default;

CrosAppsApiInfo& CrosAppsApiInfo::AddAllowlistedOrigins(
    std::initializer_list<std::string_view> additions) {
  std::vector<url::Origin> new_origins;
  new_origins.reserve(additions.size());

  base::ranges::transform(additions, std::back_inserter(new_origins),
                          [](std::string_view str) {
                            auto ret = url::Origin::Create(GURL(str));
                            // The provided literal string be the same as the
                            // parsed origin. It shouldn't contain extra parts
                            // (e.g. URL path and query) that aren't part of the
                            // origin.
                            CHECK_EQ(ret.GetURL().spec(), str);
                            return ret;
                          });

  AddAllowlistedOrigins(std::move(new_origins));
  return *this;
}

CrosAppsApiInfo& CrosAppsApiInfo::AddAllowlistedOrigins(
    const std::vector<url::Origin>& additions) {
  for (const auto& origin : additions) {
    CHECK(!origin.opaque());
    CHECK(IsUrlEligibleForCrosAppsApis(origin.GetURL()));
  }

  // For-loop with insert() is O(N^2) because we use a flat_set.
  // Instead, merge two vectors then sort.
  std::vector<url::Origin> merged_origins;
  merged_origins.reserve(allowed_origins_.size() + additions.size());

  base::ranges::copy(allowed_origins_, std::back_inserter(merged_origins));
  base::ranges::copy(additions, std::back_inserter(merged_origins));

  allowed_origins_ = std::move(merged_origins);
  return *this;
}

CrosAppsApiInfo& CrosAppsApiInfo::SetRequiredFeatures(
    std::initializer_list<std::reference_wrapper<const base::Feature>>
        features) {
  required_features_ = decltype(required_features_)(features);
  return *this;
}
