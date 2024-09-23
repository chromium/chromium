// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFO_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFO_H_

#include <functional>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/mojom/runtime_feature_state//runtime_feature.mojom-forward.h"
#include "url/origin.h"

// Identifies ChromeOS Apps APIs. Use this to query systems like API access
// control.
using CrosAppsApiId = blink::mojom::RuntimeFeature;

// CrosAppsApiInfo carries information about a ChromeOS Apps API that's required
// for the rest of ChromeOS Apps system to function.
class CrosAppsApiInfo {
 public:
  using EnableBlinkRuntimeFeatureFunction =
      void (blink::RuntimeFeatureStateContext::*)(bool);

  CrosAppsApiInfo(CrosAppsApiId api_id,
                  EnableBlinkRuntimeFeatureFunction enable_fn);

  CrosAppsApiInfo(const CrosAppsApiInfo&) = delete;
  CrosAppsApiInfo& operator=(const CrosAppsApiInfo&) = delete;

  CrosAppsApiInfo(CrosAppsApiInfo&&);
  CrosAppsApiInfo& operator=(CrosAppsApiInfo&&);

  ~CrosAppsApiInfo();

  // `AddAllowlistedOrigins` adds `origins` to the list of origins where the API
  // will be enabled.
  CrosAppsApiInfo& AddAllowlistedOrigins(
      std::initializer_list<std::string_view> origins);
  CrosAppsApiInfo& AddAllowlistedOrigins(
      const std::vector<url::Origin>& origins);

  // `SetRequiredFeatures` sets the list of `base::Feature`s that all need to be
  // enabled for the API to be enabled.
  //
  // If `features` is an empty list (or this method isn't called), it means the
  // API isn't gated behind any feature, and will be enabled if other conditions
  // match (e.g. the origin is allowlisted).
  CrosAppsApiInfo& SetRequiredFeatures(
      std::initializer_list<std::reference_wrapper<const base::Feature>>
          features);

  // Returns the enum that represents this API.
  CrosAppsApiId api_id() const { return api_id_; }

  // Returns the function that should be called on RuntimeFeatureStateContext to
  // enable the Blink runtime feature.
  EnableBlinkRuntimeFeatureFunction enable_blink_runtime_feature_fn() const {
    return enable_blink_runtime_feature_fn_;
  }

  // Returns the list of origins that could have access to the API.
  const base::flat_set<url::Origin>& allowed_origins() const {
    return allowed_origins_;
  }

  // Returns the list of `base::Feature`s that must all be enabled for the API
  // to be enabled.
  const std::vector<std::reference_wrapper<const base::Feature>>&
  required_features() const {
    return required_features_;
  }

 private:
  CrosAppsApiId api_id_;
  // TODO(b/309556977): Remove enablement function when Runtime Feature State
  // supports a generic SetEnabled() method.
  EnableBlinkRuntimeFeatureFunction enable_blink_runtime_feature_fn_;

  base::flat_set<url::Origin> allowed_origins_;
  std::vector<std::reference_wrapper<const base::Feature>> required_features_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFO_H_
