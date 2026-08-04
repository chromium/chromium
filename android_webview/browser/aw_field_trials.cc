// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trials.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_switches.h"
#include "base/allocator/partition_alloc_features.h"
#include "base/base_paths_android.h"
#include "base/check.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "components/content_settings/core/common/features.h"
#include "components/history/core/browser/features.h"
#include "components/input/features.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/persistent_histograms.h"
#include "components/payments/content/android/payment_feature_map.h"
#include "components/permissions/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/variations/feature_overrides.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_features.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "storage/browser/blob/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/android/ui_android_features.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"

void AwFieldTrials::OnVariationsSetupComplete() {
  // Persistent histograms must be enabled ASAP, but depends on Features.
  android_webview::AwMetricsServiceClient* metrics_service_client =
      android_webview::AwMetricsServiceClient::GetInstance();
  metrics_service_client->SetUpMetricsDir();
  InstantiatePersistentHistogramsWithFeaturesAndCleanup(
      metrics_service_client->GetMetricsDir());
}

// TODO(crbug.com/40271903): Consider to migrate all WebView feature overrides
// from the AwMainDelegate to the new mechanism here.
void AwFieldTrials::RegisterFeatureOverrides(base::FeatureList* feature_list) {
  if (!feature_list) {
    return;
  }
  variations::FeatureOverrides aw_feature_overrides(*feature_list);

  // Disable third-party storage partitioning on WebView.
  aw_feature_overrides.DisableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // Disable fetching partitioned Blob URL on WebView.
  aw_feature_overrides.DisableFeature(
      ::features::kBlockCrossPartitionBlobUrlFetching);

  // DISABLED_TEMPORARY: crbug.com/445202443. There are some test cases
  // need to be fixed before enabling this feature flag for android.
  aw_feature_overrides.DisableFeature(
      blink::features::kAboutBlankPageRespectsDarkModeOnUserAction);

  // DISABLED_TEMPORARY: crbug.com/444669046. Remove this once WebView
  // experiment has concluded.
  aw_feature_overrides.DisableFeature(
      input::features::kUpdateScrollPredictorInputMapping);

  // DISABLED_TEMPORARY: crbug.com/444669046. Disable
  // ScrollPredictorEnhancements refined prediction on WebView by default so it
  // can be rolled out via Finch.
  aw_feature_overrides.DisableFeature(
      blink::features::kScrollPredictorRefinedHasPrediction);

  // DISABLED_INCOMPATIBLE: InputVizard is disabled on WebView as it is a
  // Chrome-only feature that moves input handling to the VizCompositor
  // thread, which is out of scope for WebView's Synchronous Compositor
  // architecture.
  aw_feature_overrides.DisableFeature(input::features::kInputOnViz);
  aw_feature_overrides.DisableFeature(
      input::features::kInputVizardSpeculativeTransfer);

  // Disable enforcing `noopener` on Blob URL navigations on WebView.
  aw_feature_overrides.DisableFeature(
      blink::features::kEnforceNoopenerOnBlobURLNavigation);

  // DISABLED_TEMPORARY: https://crbug.com/40593023
  aw_feature_overrides.DisableFeature(
      ::features::kDefaultPassthroughCommandDecoder);

  // DISABLED_TEMPORARY: https://crbug.com/1493153. HDR does not support webview
  // yet.
  aw_feature_overrides.DisableFeature(ui::kAndroidHDR);

  // Disable launch_handler on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kWebAppLaunchQueue);

  // Disable Reducing User Agent minor version on WebView.
  aw_feature_overrides.DisableFeature(
      blink::features::kReduceUserAgentMinorVersion);

  // Disable fenced frames on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kFencedFrames);

  // Disable FLEDGE on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kAdInterestGroupAPI);
  aw_feature_overrides.DisableFeature(blink::features::kFledge);

  // DISABLED_OTHER: Disable low latency overlay for WebView. There is
  // currently no plan to enable these optimizations in WebView though they are
  // not fundamentally impossible.
  aw_feature_overrides.DisableFeature(
      blink::features::kLowLatencyUsageSupportedForCanvas);

  // Disable Shared Storage on WebView.
  aw_feature_overrides.DisableFeature(network::features::kSharedStorageAPI);

  // Disable scrollbar-color on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kScrollbarColor);

  // Disable scrollbar-width on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kScrollbarWidth);

  // Disable Populating the VisitedLinkDatabase on WebView.
  aw_feature_overrides.DisableFeature(history::kPopulateVisitedLinkDatabase);

  // DISABLED_OTHER: WebView uses kWebViewVulkan to control vulkan.
  // Preemptively disable kVulkan in case it becomes enabled by default.
  aw_feature_overrides.DisableFeature(::features::kVulkan);

  // WebView does not support web-app (service-worker) based payment apps for
  // Payment Request.
  aw_feature_overrides.DisableFeature(::features::kServiceWorkerPaymentApps);

  // Payment Request on WebView does not send down the deprecated parameters to
  // Android payment apps.
  aw_feature_overrides.EnableFeature(
      ::payments::android::kAndroidPaymentIntentsOmitDeprecatedParameters);

  // WebView does not support Secure Payment Confirmation, and thus should not
  // expose the PaymentRequest.securePaymentConfirmationAvailability API.
  aw_feature_overrides.DisableFeature(
      blink::features::kSecurePaymentConfirmationAvailabilityAPI);

  // WebView does not support Secure Payment Confirmation, and thus should not
  // expose the PaymentRequest.securePaymentConfirmationCapabilities API.
  aw_feature_overrides.DisableFeature(
      blink::features::kSecurePaymentConfirmationCapabilities);

  // WebView does not support handling payment links.
  aw_feature_overrides.DisableFeature(blink::features::kPaymentLinkDetection);

  // WebView does not support overlay fullscreen yet for video overlays.
  aw_feature_overrides.DisableFeature(media::kOverlayFullscreenVideo);

  // DISABLED_NEEDS_API: WebView does not support EME persistent license yet,
  // because it's not clear on how user can remove persistent media licenses
  // from UI.
  aw_feature_overrides.DisableFeature(media::kMediaDrmPersistentLicense);

  // DISABLED_INCOMPATIBLE: WebView does not support multiple processes, so
  // don't try to call some MediaDrm APIs in a separate process.
  aw_feature_overrides.DisableFeature(media::kMediaDrmQueryInSeparateProcess);

  aw_feature_overrides.DisableFeature(::features::kBackgroundFetch);

  // DISABLED_OTHER: SurfaceControl is controlled by kWebViewSurfaceControl
  // flag.
  aw_feature_overrides.DisableFeature(::features::kAndroidSurfaceControl);

  // DISABLED_TEMPORARY: crbug.com/40627649. WebOTP is not yet supported
  // on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebOTP);

  // DISABLED_TEMPORARY: crbug.com/40652382. WebXR is not yet supported
  // on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebXr);

  // DISABLED_TEMPORARY: crbug.com/40831925. Digital Goods API is not yet
  // supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kDigitalGoodsApi);

  // DISABLED_TEMPORARY: COOP is not supported on WebView yet. See:
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/XBKAGb2_7uAi.
  aw_feature_overrides.DisableFeature(
      network::features::kCrossOriginOpenerPolicy);

  aw_feature_overrides.DisableFeature(::features::kInstalledApp);

  aw_feature_overrides.DisableFeature(::features::kPeriodicBackgroundSync);

  // Disable dr-dc on webview.
  aw_feature_overrides.DisableFeature(::features::kEnableDrDc);

  // DISABLED_TEMPORARY: crbug.com/40703318. Web Bluetooth is not yet
  // supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebBluetooth);

  // DISABLED_TEMPORARY: crbug.com/41441927. WebUSB is not yet supported
  // on WebView.
  aw_feature_overrides.DisableFeature(::features::kWebUsb);

  // Disable Web Serial API on WebView.
  aw_feature_overrides.DisableFeature(blink::features::kWebSerialAPI);

  // Disable key pinning enforcement on webview.
  aw_feature_overrides.DisableFeature(
      net::features::kStaticKeyPinningEnforcement);

  // DISABLED_TEMPORARY: FedCM is not yet supported on WebView.
  aw_feature_overrides.DisableFeature(::features::kFedCm);

  // DISABLED_TEMPORARY: Email Verification Protocol is not yet supported on
  // WebView.
  aw_feature_overrides.DisableFeature(::features::kEmailVerificationProtocol);

  // DISABLED_TEMPORARY: temporarily disable Digital Credentials API on WebView
  // while we figure out a long-term plan for exposing this to apps.
  aw_feature_overrides.DisableFeature(
      ::features::kWebIdentityDigitalCredentials);
  aw_feature_overrides.DisableFeature(
      ::features::kWebIdentityDigitalCredentialsCreation);

  // DISABLED_NEEDS_API: crbug.com/40272633. Web MIDI permission prompt
  // needs better support for web permissions.
  aw_feature_overrides.DisableFeature(blink::features::kBlockMidiByDefault);

  // Disable device posture API as the framework implementation causes
  // AwContents to leak in apps that don't call destroy().
  aw_feature_overrides.DisableFeature(blink::features::kDevicePosture);
  aw_feature_overrides.DisableFeature(blink::features::kViewportSegments);

  // PaintHolding for OOPIFs. This should be a no-op since WebView doesn't use
  // site isolation but field trial testing doesn't indicate that. Revisit when
  // enabling site isolation. See crbug.com/356170748.
  aw_feature_overrides.DisableFeature(blink::features::kPaintHoldingForIframes);

  // Disabling this feature for WebView, since it can switch focus when scrolled
  // in cases with multiple views which can trigger HTML focus changes that
  // aren't intended. See crbug.com/378779896, crbug.com/373672168 for more
  // details.
  aw_feature_overrides.DisableFeature(
      ::features::kFocusRenderWidgetHostViewAndroidOnActionDown);

  // DISABLED_NEEDS_API: Disabling the permission element as it needs
  // embedder support in order to function and this feature is blocked by
  // WebView needing better support for web permissions.
  aw_feature_overrides.DisableFeature(blink::features::kGeolocationElement);
  aw_feature_overrides.DisableFeature(blink::features::kUserMediaElement);
  aw_feature_overrides.DisableFeature(blink::features::kInstallElement);
  aw_feature_overrides.DisableFeature(
      blink::features::kCameraAndMicrophoneElements);

  // Delete Incidental Party State (DIPS) feature is not yet supported on
  // WebView.
  aw_feature_overrides.DisableFeature(::features::kBtm);

  // These features have shown performance improvements in WebView but not some
  // other platforms.
  aw_feature_overrides.EnableFeature(features::kEnsureExistingRendererAlive);
  aw_feature_overrides.EnableFeature(blink::features::kThreadedBodyLoader);
  aw_feature_overrides.EnableFeature(blink::features::kThreadedPreloadScanner);
  aw_feature_overrides.EnableFeature(blink::features::kPrecompileInlineScripts);

  // DISABLED_TEMPORARY: crbug.com/336852432. This feature has not been
  // experimented with yet on WebView.
  aw_feature_overrides.DisableFeature(
      blink::features::kNavigationPredictorNewViewportFeatures);

  // DISABLED_INCOMPATIBLE: This feature is global for the process and thus
  // should not be enabled by WebView because this will interfere with the host
  // app.
  aw_feature_overrides.DisableFeature(
      base::features::kPartitionAllocMemoryTagging);

  // Disable Topics on WebView.
  aw_feature_overrides.DisableFeature(network::features::kBrowsingTopics);

  // DISABLED_INCOMPATIBLE: Sharing ANGLE's Vulkan queue is not supported on
  // WebView.
  aw_feature_overrides.DisableFeature(::features::kVulkanFromANGLE);

  // DISABLED_TEMPORARY: crbug.com/371512561. This feature has not been
  // experimented with yet on WebView. Disable this feature for WebView only if
  // webview itself is using GLES.
  aw_feature_overrides.DisableFeature(::features::kDefaultANGLEVulkan);

  // Partitioned :visited links history is not supported on WebView.
  aw_feature_overrides.DisableFeature(
      blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks);

  // Explicitly disable PrefetchProxy instead of relying only on passing an
  // empty URL.
  aw_feature_overrides.DisableFeature(features::kPrefetchProxy);

  // Document Picture-in-Picture API is not supported on WebView.
  aw_feature_overrides.DisableFeature(
      blink::features::kDocumentPictureInPictureAPI);

  // AAudio per-stream device selection is not supported on WebView.
  aw_feature_overrides.DisableFeature(
      features::kAAudioPerStreamDeviceSelection);

  // Local Network Access restrictions should not be enforced in WebView.
  // The LNA permission is auto-granted in WebView, but the permission
  // policy currently blocks iframes from using it. crbug.com/442879527
  aw_feature_overrides.DisableFeature(
      network::features::kLocalNetworkAccessChecks);

  // Disable background media for WebView, until we have consensus on long-term
  // behavior crbug.com/453706851
  aw_feature_overrides.DisableFeature(
      features::kAndroidEnableBackgroundMediaLargeFormFactors);

  // DISABLED_INCOMPATIBLE: Disable ExtendedReportingRemovePrefDependency for
  // WebView, because WebView doesn't support ESB
  aw_feature_overrides.DisableFeature(
      safe_browsing::kExtendedReportingRemovePrefDependency);

  // SystemTracing is enabled by default only in WebView for now.
  aw_feature_overrides.EnableFeature(features::kEnablePerfettoSystemTracing);

  // DISABLED_OTHER: Deemed that performance benefit is not worth the
  // stability cost. See crbug.com/1309151.
  aw_feature_overrides.DisableFeature(::features::kGpuShaderDiskCache);

  // DISABLED_TEMPORARY: https://crbug.com/399642827. GpuPersistentCache is
  // enabled by default on Clank but not on WebView yet. Disable it by default
  // so it can be rolled out using Finch.
  aw_feature_overrides.DisableFeature(::features::kGpuPersistentCache);

  // Don't pass the data about browser window position on screen to WebView.
  aw_feature_overrides.DisableFeature(ui::kAndroidUseCorrectWindowBounds);

  // DISABLED_NEEDS_API: As WebSettings.setAllowContentAccess() allows this to
  // be controlled by the WebView's host, we keep the old behavior for
  // content:// URLs. This has to stay disabled for as long as
  // setAllowContentAccess() is supported.
  aw_feature_overrides.DisableFeature(blink::features::kContentSchemeIsLocal);

  // DISABLED_TEMPORARY: crbug.com/489450060. Disable DirectReceiver on
  // Viz for WebView until its Viz thread is updated to handle IO.
  aw_feature_overrides.DisableFeature(
      ::features::kVizDirectCompositorThreadIpcFrameSinkManager);

  // DISABLED_TEMPORARY: Disable `PrefetchRequestStatusListenerAsync` on
  // WebView to run an experiment on WebView.
  aw_feature_overrides.DisableFeature(
      ::features::kPrefetchRequestStatusListenerAsync);

  // Disable prefetch ahead of prerender on WebView.
  aw_feature_overrides.DisableFeature(
      ::features::kPrerender2FallbackPrefetchSpecRules);

  // Disable approximate geolocation permission on WebView for now because the
  // workaround for crbug.com/502587667 will break CTS tests for Geolocation.
  aw_feature_overrides.DisableFeature(
      content_settings::features::kApproximateGeolocationPermission);

  // Disable HappyEyeballsV2 for WebView for now.
  // DISABLED_TEMPORARY: crbug.com/529948671
  aw_feature_overrides.DisableFeature(net::features::kHappyEyeballsV2);

  // DISABLED_TEMPORARY: crbug.com/515084572
  aw_feature_overrides.DisableFeature(
      net::features::kCloseQuicSessionsOnPreFreeze);
}

void AwFieldTrials::EnableRuntimeMutableFeatures(
    base::FeatureList* feature_list) {
  // Add calls to enable runtime-mutable features here.
}
