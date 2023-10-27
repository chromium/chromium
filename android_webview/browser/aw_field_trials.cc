// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trials.h"

#include "base/base_paths_android.h"
#include "base/feature_list.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "components/metrics/persistent_histograms.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/android/ui_android_features.h"

namespace {

class AwFeatureOverrides {
 public:
  AwFeatureOverrides() = default;

  AwFeatureOverrides(const AwFeatureOverrides& other) = delete;
  AwFeatureOverrides& operator=(const AwFeatureOverrides& other) = delete;

  ~AwFeatureOverrides() = default;

  // Enable a feature with WebView-specific override.
  void EnableFeature(const base::Feature& feature) {
    overrides.emplace_back(
        std::cref(feature),
        base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE);
  }

  // Disable a feature with WebView-specific override.
  void DisableFeature(const base::Feature& feature) {
    overrides.emplace_back(
        std::cref(feature),
        base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
  }

  void RegisterOverrides(base::FeatureList* feature_list) {
    feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  }

 private:
  std::vector<base::FeatureList::FeatureOverrideInfo> overrides;
};

}  // namespace

void AwFieldTrials::OnVariationsSetupComplete() {
  // Persistent histograms must be enabled ASAP, but depends on Features.
  base::FilePath metrics_dir;
  if (base::PathService::Get(base::DIR_ANDROID_APP_DATA, &metrics_dir)) {
    InstantiatePersistentHistogramsWithFeaturesAndCleanup(metrics_dir);
  } else {
    NOTREACHED();
  }
}

// TODO(crbug.com/1453407): Consider to migrate all WebView feature overrides
// from the AwMainDelegate to the new mechanism here.
void AwFieldTrials::RegisterFeatureOverrides(base::FeatureList* feature_list) {
  AwFeatureOverrides aw_feature_overrides;

  // Disable user-agent client hints on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kUserAgentClientHint);

  // Disable network-change migration on WebView due to crbug.com/1430082.
  aw_feature_overrides.DisableFeature(
      net::features::kMigrateSessionsOnNetworkChangeV2);

  // HDR does not support webview yet. See crbug.com/1493153 for an explanation.
  aw_feature_overrides.DisableFeature(ui::kAndroidHDR);

  aw_feature_overrides.RegisterOverrides(feature_list);
}
