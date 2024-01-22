// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_REGISTRY_H_

#include <string_view>

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_frame_context.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature.mojom-forward.h"

class Profile;
namespace content {
class NavigationHandle;
}

// CrosAppsApiRegistry provides an read-only interface to query access control
// information about ChromeOS Apps APIs.
//
// To perform security checks, use helper methods in CrosAppsApiAccessControl.
// To modify the registry, use CrosAppsApiMutableRegistry.
class CrosAppsApiRegistry {
 public:
  // Returns a lazily constructed API registry that's attached to `profile`. The
  // returned registry is valid until `profile` destructs.
  static const CrosAppsApiRegistry& GetInstance(Profile* profile);

  // Returns whether the API identified by `api_feature` can be enabled in the
  // profile where `this` registry was retrieved from.
  //
  // This performs JavaScript context independent checks that doesn't require
  // frame information. For example, base::Feature flags and Profile types.
  virtual bool CanEnableApi(
      const blink::mojom::RuntimeFeature api_feature) const = 0;

  // Return a list of functions that should be called on
  // RuntimeFeatureStateContext to enable the blink runtime features for a given
  // frame that belongs to a ChromeOS App.
  //
  // The returned function should be called on RuntimeFeatureStateContext of a
  // NavigationHandle.
  virtual std::vector<CrosAppsApiInfo::EnableBlinkRuntimeFeatureFunction>
  GetBlinkFeatureEnablementFunctionsForFrame(
      const CrosAppsApiFrameContext& api_context) const = 0;

  // Returns whether the given API identified by `api_feature` should be enabled
  // for `api_context`.
  virtual bool IsApiEnabledForFrame(
      const blink::mojom::RuntimeFeature api_feature,
      const CrosAppsApiFrameContext& api_context) const = 0;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_REGISTRY_H_
