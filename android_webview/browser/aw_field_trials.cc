// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trials.h"

#include "android_webview/common/aw_switches.h"
#include "base/base_paths_android.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
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
#include "mojo/public/cpp/bindings/features.h"
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
    for (const auto& field_trial_override : field_trial_overrides_) {
      feature_list_->RegisterFieldTrialOverride(
          field_trial_override.feature->name,
          field_trial_override.override_state,
          field_trial_override.field_trial);
    }
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

  // Enable or disable a feature with a field trial. This can be used for
  // setting feature parameters.
  void OverrideFeatureWithFieldTrial(
      const base::Feature& feature,
      base::FeatureList::OverrideState override_state,
      base::FieldTrial* field_trial) {
    field_trial_overrides_.emplace_back(FieldTrialOverride{
        .feature = raw_ref(feature),
        .override_state = override_state,
        .field_trial = field_trial,
    });
  }

 private:
  struct FieldTrialOverride {
    raw_ref<const base::Feature> feature;
    base::FeatureList::OverrideState override_state;
    raw_ptr<base::FieldTrial> field_trial;
  };

  base::raw_ref<base::FeatureList> feature_list_;
  std::vector<base::FeatureList::FeatureOverrideInfo> overrides_;
  std::vector<FieldTrialOverride> field_trial_overrides_;
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

// TODO(crbug.com/40271903): Consider to migrate all WebView feature overrides
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

  // Disable FLEDGE on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kAdInterestGroupAPI);
  aw_feature_overrides.DisableFeature(blink::features::kFledge);

  // Disable low latency overlay for WebView. There is currently no plan to
  // enable these optimizations in WebView though they are not fundamentally
  // impossible.
  aw_feature_overrides.DisableFeature(
      blink::features::kLowLatencyCanvas2dImageChromium);
  aw_feature_overrides.DisableFeature(
      blink::features::kLowLatencyWebGLImageChromium);

  // Disable Shared Storage on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kSharedStorageAPI);
  aw_feature_overrides.DisableFeature(blink::features::kSharedStorageAPIM125);

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

  // WebView does not support multiple processes, so don't try to call some
  // MediaDrm APIs in a separate process.
  aw_feature_overrides.DisableFeature(
      media::kAllowMediaCodecCallsInSeparateProcess);

  aw_feature_overrides.DisableFeature(::features::kBackgroundFetch);

  // SurfaceControl is controlled by kWebViewSurfaceControl flag.
  aw_feature_overrides.DisableFeature(::features::kAndroidSurfaceControl);

  // TODO(crbug.com/40627649): WebOTP is not yet supported on
  // WebView.
  aw_feature_overrides.DisableFeature(::features::kWebOTP);

  // TODO(crbug.com/40652382): WebXR is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebXr);

  // TODO(crbug.com/40831925): Digital Goods API is not yet supported
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

  // TODO(crbug.com/40703318): Web Bluetooth is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebBluetooth);

  // TODO(crbug.com/41441927): WebUSB is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebUsb);

  // Disable TFLite based language detection on webview until webview supports
  // ML model delivery via Optimization Guide component.
  // TODO(crbug.com/40819484): Enable the feature on Webview.
  aw_feature_overrides.DisableFeature(
      ::translate::kTFLiteLanguageDetectionEnabled);

  // Disable key pinning enforcement on webview.
  aw_feature_overrides.DisableFeature(
      net::features::kStaticKeyPinningEnforcement);

  // FedCM is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kFedCm);

  // TODO(crbug.com/40272633): Web MIDI permission prompt for all usage.
  aw_feature_overrides.DisableFeature(blink::features::kBlockMidiByDefault);

  // Disable device posture API as the framework implementation causes
  // AwContents to leak in apps that don't call destroy().
  aw_feature_overrides.DisableFeature(blink::features::kDevicePosture);
  aw_feature_overrides.DisableFeature(blink::features::kViewportSegments);

  // PaintHolding for OOPIFs. This should be a no-op since WebView doesn't use
  // site isolation but field trial testing doesn't indicate that. Revisit when
  // enabling site isolation. See crbug.com/356170748.
  aw_feature_overrides.DisableFeature(blink::features::kPaintHoldingForIframes);

  // Since Default Nav Transition does not support WebView yet, disable the
  // LocalSurfaceId increment flag. TODO(crbug.com/361600214): Re-enable for
  // WebView when we start introducing this feature.
  aw_feature_overrides.DisableFeature(
      blink::features::kIncrementLocalSurfaceIdForMainframeSameDocNavigation);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kDebugBsa)) {
    // Feature parameters can only be set via a field trial.
    const char kTrialName[] = "StudyDebugBsa";
    const char kGroupName[] = "GroupDebugBsa";
    base::FieldTrial* field_trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
    // If field_trial is null, there was some unexpected name conflict.
    CHECK(field_trial);
    base::FieldTrialParams params;
    params.emplace(net::features::kIpPrivacyTokenServer.name,
                   "https://staging-phosphor-pa.sandbox.googleapis.com");
    base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
    aw_feature_overrides.OverrideFeatureWithFieldTrial(
        net::features::kEnableIpProtectionProxy,
        base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE, field_trial);
    aw_feature_overrides.EnableFeature(network::features::kMaskedDomainList);
  }

  // Delete Incidental Party State (DIPS) feature is not yet supported on
  // WebView.
  // TODO(b/344852824): Enable the feature for WebView
  aw_feature_overrides.DisableFeature(::features::kDIPS);

  // Async Safe Browsing check will be rolled out together with
  // kHashPrefixRealTimeLookups on WebView.
  aw_feature_overrides.DisableFeature(
      safe_browsing::kSafeBrowsingAsyncRealTimeCheck);
  aw_feature_overrides.DisableFeature(
      safe_browsing::kHashPrefixRealTimeLookups);

  // WebView does not currently support the Permissions API (crbug.com/490120)
  aw_feature_overrides.DisableFeature(::features::kWebPermissionsApi);

  // TODO(crbug.com/41492947): See crrev.com/c/5744034 for details, but I was
  // unable to add this feature to fieldtrial_testing_config and pass all tests.
  aw_feature_overrides.EnableFeature(blink::features::kElementGetInnerHTML);

  // These features have shown performance improvements in WebView but not some
  // other platforms.
  aw_feature_overrides.EnableFeature(features::kEnsureExistingRendererAlive);
  aw_feature_overrides.EnableFeature(blink::features::kThreadedBodyLoader);
  aw_feature_overrides.EnableFeature(blink::features::kThreadedPreloadScanner);
  aw_feature_overrides.EnableFeature(blink::features::kPrecompileInlineScripts);

  // This feature has not been experimented with yet on WebView.
  // TODO(crbug.com/336852432): Enable this feature for WebView.
  aw_feature_overrides.DisableFeature(
      blink::features::kNavigationPredictorNewViewportFeatures);
}
