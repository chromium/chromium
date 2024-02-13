// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trials.h"

#include "base/base_paths_android.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "components/history/core/browser/features.h"
#include "components/metrics/persistent_histograms.h"
#include "components/permissions/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/translate/core/common/translate_util.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_features.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/android/ui_android_features.h"
#include "ui/gl/gl_features.h"

namespace {

class AwFeatureOverrides {
 public:
  explicit AwFeatureOverrides(base::FeatureList& feature_list)
      : feature_list_(feature_list) {}

  AwFeatureOverrides(const AwFeatureOverrides& other) = delete;
  AwFeatureOverrides& operator=(const AwFeatureOverrides& other) = delete;

  ~AwFeatureOverrides() {
    feature_list_->RegisterExtraFeatureOverrides(std::move(overrides_));
  }

  // Enable a feature with WebView-specific override.
  void EnableFeature(const base::Feature& feature) {
    overrides_.emplace_back(
        std::cref(feature),
        base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE);
  }

  // Disable a feature with WebView-specific override.
  void DisableFeature(const base::Feature& feature) {
    overrides_.emplace_back(
        std::cref(feature),
        base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
  }

 private:
  base::raw_ref<base::FeatureList> feature_list_;
  std::vector<base::FeatureList::FeatureOverrideInfo> overrides_;
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
  if (!feature_list) {
    return;
  }
  AwFeatureOverrides aw_feature_overrides(*feature_list);

  // Disable third-party storage partitioning on WebView.
  aw_feature_overrides.DisableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // Disable the passthrough on WebView.
  aw_feature_overrides.DisableFeature(
      ::features::kDefaultPassthroughCommandDecoder);

  // HDR does not support webview yet. See crbug.com/1493153 for an explanation.
  aw_feature_overrides.DisableFeature(ui::kAndroidHDR);

  // Disable Reducing User Agent minor version on WebView.
  aw_feature_overrides.DisableFeature(
      blink::features::kReduceUserAgentMinorVersion);

  // Disable fenced frames on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kFencedFrames);

  // Disable low latency overlay for WebView. There is currently no plan to
  // enable these optimizations in WebView though they are not fundamentally
  // impossible.
  aw_feature_overrides.DisableFeature(
      blink::features::kLowLatencyCanvas2dImageChromium);
  aw_feature_overrides.DisableFeature(
      blink::features::kLowLatencyWebGLImageChromium);

  // Disable Shared Storage on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kSharedStorageAPI);

  // Disable scrollbar-color on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kScrollbarColor);

  // Disable scrollbar-width on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kScrollbarWidth);

  // Disable Populating the VisitedLinkDatabase on WebView.
  aw_feature_overrides.DisableFeature(history::kPopulateVisitedLinkDatabase);

  // WebView uses kWebViewVulkan to control vulkan. Pre-emptively disable
  // kVulkan in case it becomes enabled by default.
  aw_feature_overrides.DisableFeature(::features::kVulkan);

  aw_feature_overrides.DisableFeature(::features::kWebPayments);
  aw_feature_overrides.DisableFeature(::features::kServiceWorkerPaymentApps);

  // WebView does not support overlay fullscreen yet for video overlays.
  aw_feature_overrides.DisableFeature(media::kOverlayFullscreenVideo);

  // WebView does not support EME persistent license yet, because it's not
  // clear on how user can remove persistent media licenses from UI.
  aw_feature_overrides.DisableFeature(media::kMediaDrmPersistentLicense);

  aw_feature_overrides.DisableFeature(::features::kBackgroundFetch);

  // SurfaceControl is controlled by kWebViewSurfaceControl flag.
  aw_feature_overrides.DisableFeature(::features::kAndroidSurfaceControl);

  // TODO(https://crbug.com/963653): WebOTP is not yet supported on
  // WebView.
  aw_feature_overrides.DisableFeature(::features::kWebOTP);

  // TODO(https://crbug.com/1012899): WebXR is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebXr);

  // TODO(https://crbug.com/1312827): Digital Goods API is not yet supported
  // on WebView.
  aw_feature_overrides.DisableFeature(::features::kDigitalGoodsApi);

  aw_feature_overrides.DisableFeature(::features::kDynamicColorGamut);

  // COOP is not supported on WebView yet. See:
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/XBKAGb2_7uAi.
  aw_feature_overrides.DisableFeature(
      network::features::kCrossOriginOpenerPolicy);

  aw_feature_overrides.DisableFeature(::features::kInstalledApp);

  aw_feature_overrides.DisableFeature(::features::kPeriodicBackgroundSync);

  // Disabled until viz scheduling can be improved.
  aw_feature_overrides.DisableFeature(
      ::features::kUseSurfaceLayerForVideoDefault);

  // Disable dr-dc on webview.
  aw_feature_overrides.DisableFeature(::features::kEnableDrDc);

  // TODO(crbug.com/1100993): Web Bluetooth is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebBluetooth);

  // TODO(crbug.com/933055): WebUSB is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebUsb);

  // Disable TFLite based language detection on webview until webview supports
  // ML model delivery via Optimization Guide component.
  // TODO(crbug.com/1292622): Enable the feature on Webview.
  aw_feature_overrides.DisableFeature(
      ::translate::kTFLiteLanguageDetectionEnabled);

  // Disable key pinning enforcement on webview.
  aw_feature_overrides.DisableFeature(
      net::features::kStaticKeyPinningEnforcement);

  // FedCM is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kFedCm);

  // Storage Access permission prompts are not supported on WebView.
  aw_feature_overrides.DisableFeature(
      permissions::features::kPermissionStorageAccessAPI);
}
