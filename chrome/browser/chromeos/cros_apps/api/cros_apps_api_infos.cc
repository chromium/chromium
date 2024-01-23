// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_infos.h"

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature.mojom.h"

// TODO(b/309556977): Replace macro with something better once Runtime Feature
// State supports a generic SetEnabled() method.
#define DEFINE_CROS_APPS_API(api_list, name)                                 \
  (list.emplace(list.end(), CrosAppsApiId::k##name,                          \
                CrosAppsApiInfo(                                             \
                    CrosAppsApiId::k##name,                                  \
                    &blink::RuntimeFeatureStateContext::Set##name##Enabled)) \
       ->second)

std::vector<std::pair<CrosAppsApiId, CrosAppsApiInfo>>
CreateDefaultCrosAppsApiInfo() {
  std::vector<std::pair<CrosAppsApiId, CrosAppsApiInfo>> list;

  DEFINE_CROS_APPS_API(list, BlinkExtensionDiagnostics)
      .SetRequiredFeatures({chromeos::features::kBlinkExtensionDiagnostics})
      .AddAllowlistedOrigins({
          // Externally visible prototype hosting website.
          "https://serve-dot-zipline.appspot.com/",
          // System info viewer isolated app.
          "isolated-app://"
          "uwsszrmaowqmxw4f262x5jozzhe5bc4tefqfa5lado674o462aoaaaic/",
      });

  DEFINE_CROS_APPS_API(list, BlinkExtensionChromeOSKiosk)
      .SetRequiredFeatures({chromeos::features::kBlinkExtensionKiosk});

  return list;
}

#undef DEFINE_CROS_APPS_API
