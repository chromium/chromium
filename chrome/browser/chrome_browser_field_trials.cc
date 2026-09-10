// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/feed/feed_feature_list.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_histograms.h"
#include "components/site_isolation/features.h"
#include "components/variations/feature_overrides.h"
#include "components/version_info/version_info.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/background_thread_pool_field_trial.h"
#include "base/android/bundle_utils.h"
#include "base/task/thread_pool/environment_config.h"
#include "build/android_buildflags.h"
#include "cc/base/features.h"
#include "chrome/browser/android/flags/chrome_cached_flags.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/download/public/common/download_features.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "sandbox/policy/features.h"
#include "ui/base/ui_base_features.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/services/multidevice_setup/public/cpp/first_run_field_trial.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/check_deref.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/signin/first_run_desktop_refresh_field_trial.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() = default;

void ChromeBrowserFieldTrials::SetUpClientSideFieldTrials(
    bool has_seed,
    const variations::EntropyProviders& entropy_providers,
    base::FeatureList* feature_list) {
  // Only create the fallback trials if there isn't already a variations seed
  // being applied. This should occur during first run when first-run variations
  // isn't supported. It's assumed that, if there is a seed, then it either
  // contains the relevant studies, or is intentionally omitted, so no fallback
  // is needed. The exception is for sampling trials. Fallback trials are
  // created even if no variations seed was applied. This allows testing the
  // fallback code by intentionally omitting the sampling trial from a
  // variations seed.
  metrics::CreateFallbackSamplingTrialsIfNeeded(
      entropy_providers.default_entropy(), feature_list);
  metrics::CreateFallbackUkmSamplingTrialIfNeeded(
      entropy_providers.default_entropy(), feature_list);

#if BUILDFLAG(IS_CHROMEOS)
  if (!has_seed) {
    ash::multidevice_setup::CreateFirstRunFieldTrial(feature_list);
  }
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // This trial is client controlled on Mac and Linux because the first run
  // experience is shown on the very first run of Chrome. These platforms do not
  // support variations seed on the first run.
  if (first_run::IsChromeFirstRun()) {
    signin::CreateFirstRunDesktopRefreshFieldTrial(
        CHECK_DEREF(feature_list), entropy_providers.default_entropy());
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

void ChromeBrowserFieldTrials::RegisterSyntheticTrials() {
#if BUILDFLAG(IS_ANDROID)
  {
    auto trial_info =
        base::android::BackgroundThreadPoolFieldTrial::GetTrialInfo();
    if (trial_info.has_value()) {
      // The annotation mode is set to |kCurrentLog| since the field trial has
      // taken effect at process startup.
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          trial_info->trial_name, trial_info->group_name,
          variations::SyntheticTrialAnnotationMode::kCurrentLog);
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserFieldTrials::RegisterFeatureOverrides(
    base::FeatureList* feature_list) {
  variations::FeatureOverrides feature_overrides(*feature_list);

  // TODO(crbug.com/552456654): Remove when rollout is complete to stable.
  if (chrome::GetChannel() != version_info::Channel::STABLE) {
    feature_overrides.EnableFeature(
        blink::features::kSingleAxisScrollContainers);
  }

#if BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // Nota bene: Anything here is expected to be short-lived, unless deemed too
  // risky to launch to non-desktop platforms. New features being added here
  // should be the exception, and not the norm. Instead, you should place the
  // override in the generic IS_ANDROID block below, guarded by an appropriate
  // runtime check.

  // Enable the "Ask Gemini" context-menu and text-selection entry points on
  // desktop Android (AL); disabled by default on other Android form factors.
  // TODO(crbug.com/545717789): Remove when rollout to phones/tablets is
  // complete.
  feature_overrides.EnableFeature(chrome::android::kClankGlicContextMenu);

  // Enables media capture (tab+window+screen sharing).
  // TODO(crbug.com/352187279): Remove when tablet rollout is complete.
  feature_overrides.EnableFeature(kAndroidMediaPicker);
  feature_overrides.EnableFeature(features::kUserMediaScreenCapturing);

  // Enable open download in new tab.
  // TODO(crbug.com/531944280): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(download::features::kOpenDownloadInNewTab);

  // Enable`save as`context menu.
  feature_overrides.EnableFeature(
      download::features::kEnableDownloadSaveAsContextMenu);

  // Enable open download in preferred app.
  // TODO(crbug.com/539965859): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(chrome::android::kOpenDownloadInPreferredApp);

  // Enable background media capturing on desktop devices.
  // TODO(crbug.com/426461170): Remove once we enable this feature for all form
  // factors. Currently we have no conclusion whether to enable this on mobile
  // phones yet.
  feature_overrides.EnableFeature(
      media::kAndroidEnableBackgroundMediaCapturing);

  // Enable WebRTC suspend on screen off for desktop devices.
  // TODO(crbug.com/533876870): Remove once Android provides a dedicated API to
  // notify WebRTC of system suspend or lid close events.
  feature_overrides.EnableFeature(media::kAndroidSuspendWebRtcOnScreenOff);

  // TODO(crbug.com/422903297): Remove when tablet rollout is complete.
  feature_overrides.EnableFeature(features::kRendererProcessLimitOnAndroid);
  // Enable V8 optimizations for high-end Android Desktop devices.
  // TODO(crbug.com/425860368): Remove when the feature is stable.
  feature_overrides.EnableFeature(features::kV8AndroidDesktopHighEndConfig);
  // TODO(crbug.com/430304112): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(
      autofill::features::kAutofillAndroidDesktopSuppressAccessoryOnEmpty);
  // TODO(crbug.com/445446479): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(
      sandbox::policy::features::kAndroidGpuSandbox);
  // Bypass the WebAudio output buffer, to reduce audio latency.
  // TODO(crbug.com/436988695): Remove when the long term solution is
  // implemented.
  feature_overrides.EnableFeature(
      blink::features::kWebAudioBypassOutputBuffering);
  // TODO(crbug.com/437004266): Remove when the feature is stable.
  feature_overrides.EnableFeature(
      features::kAlwaysUseAudioManagerOutputFramesPerBuffer);
  // TODO(crbug.com/440210010): Remove when the feature experiment is done.
  feature_overrides.EnableFeature(features::kAudioStereoInputStreamParameters);
  // Enables automatic picture-in-picture.
  // TODO(crbug.com/421608904): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(media::kAutoPictureInPictureAndroid);
  // Enables picture-in-picture in the right-click context menu.
  // TODO(crbug.com/403851785): Remove when the feature is verified to be stable
  // on desktop Android.
  feature_overrides.EnableFeature(media::kContextMenuPictureInPictureAndroid);

  // Enable Media Engagement bypass and preload for desktop Android.
  // TODO(crbug.com/490450572): Re-evaluate if we want to enable these features
  // for all Android form factors after analysis.
  feature_overrides.EnableFeature(
      media::kMediaEngagementBypassAutoplayPolicies);
  feature_overrides.EnableFeature(media::kPreloadMediaEngagementData);

  // Disables the enhanced pip transition and uses the default animation.
  // TODO(crbug.com/440384447): Remove when enhanced pip transition is fixed.
  feature_overrides.DisableFeature(media::kAllowEnhancedPipTransition);

  // Enables Document Picture-in-Picture on desktop Android; disabled on other
  // Android form factors until system fullscreen support is available
  // (crbug.com/534397738).
  feature_overrides.EnableFeature(
      blink::features::kDocumentPictureInPictureAPI);

  // Enables SVC bitrate layering for NdkVideoEncodeAccelerator on desktop
  // Android ahead of NDK r30 rollout across the rest of Android.
  feature_overrides.EnableFeature(
      media::kNdkVideoEncodeAcceleratorBitrateLayering);

  // Enables native SVC temporal layer retrieval for NdkVideoEncodeAccelerator
  // on desktop Android ahead of NDK r30 rollout across the rest of Android.
  feature_overrides.EnableFeature(media::kNdkVideoEncodeAcceleratorNativeSvc);

  // Disables fullscreen video picture-in-picture on desktop Android for desktop
  // behavior parity; deprecates the fullscreen -> swipe home -> enter PiP path.
  feature_overrides.DisableFeature(media::kFullscreenVideoPictureInPicture);

  // Enforces 2-pixel even boundary alignment for YUV SurfaceControl overlays
  // on desktop Android as a native workaround for Intel hardware scalers.
  feature_overrides.EnableFeature(features::kAndroidYuvOverlayEvenAlignment);

  // Enable by default for desktop platforms, pending a phone / foldable /
  // tablet rollout using the same flag.
  // TODO(crbug.com/442327273): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(
      autofill::features::kAutofillAndroidDesktopKeyboardAccessoryRevamp);

  // Enable ANGLE/Vulkan features.
  // TODO (crbug.com//376280554): Enable these features with runtime checks
  // instead.
  feature_overrides.EnableFeature(::features::kSkipVulkanBlocklist);
  feature_overrides.EnableFeature(::features::kDefaultANGLEVulkan);
  feature_overrides.EnableFeature(::features::kVulkanFromANGLE);
  feature_overrides.EnableFeature(::features::kDefaultPassthroughCommandDecoder);

  // Enable site-per-process by default for desktop platforms.
  // TODO(crbug.com/453856709): Remove when we determine how to ensure
  // SitePerProcess is enabled for all necessary or eligible Android devices.
  feature_overrides.EnableFeature(::features::kSitePerProcess);

  // Enable sandboxed process service limit for desktop platforms.
  // This should be on for all devices where SitePerProcess is on by default.
  // TODO(crbug.com/534420192): Remove when this is enabled by default for all
  // relevant Android devices.
  feature_overrides.EnableFeature(
      features::kSandboxedProcessServiceLimitOnAndroid);

  // By setting the kSiteIsolationEnableMemoryThresholdAndroid feature, we make
  // sure that site isolation (enabled by kSitePerProcess above) is not disabled
  // due to memory thresholds.
  // TODO(crbug.com/454695278): Find a different way to disable the site
  // isolation memory thresholds on Android desktop.
  feature_overrides.DisableFeature(
      site_isolation::features::kSiteIsolationEnableMemoryThresholdAndroid);

  // Enable all tabs to have WebContents at all times for desktop platforms.
  // TODO(crbug.com/448420873): Remove once we enable this feature for all form
  // factors. This is currently blocked by performance regressions on low-end
  // Android devices.
  feature_overrides.EnableFeature(features::kWebContentsDiscard);
  feature_overrides.EnableFeature(features::kLazyBrowserInterfaceBroker);
  feature_overrides.EnableFeature(chrome::android::kLoadAllTabsAtStartup);

  // Enable desktop tab restore logic, where some background tabs get
  // reloaded. This requires kLoadAllTabsAtStartup above to be enabled as well.
  // This is not enabled elsewhere because the desktop behavior is not desirable
  // on mobile Android.
  feature_overrides.EnableFeature(
      chrome::android::kDesktopAndroidBackgroundTabLoading);

  // Enable the ability for extensions to override chrome pages.
  // TODO(crbug.com/404069963): Remove flag when the feature is verified to be
  // stable on desktop Android.
  feature_overrides.EnableFeature(chrome::android::kChromeNativeUrlOverriding);

  // Enable desktop full screen to a screen feature flag by default for desktop
  // platforms.
  // TODO(crbug.com/417426218) Remove once feature is launched to 100% on all
  // form factors.
  feature_overrides.EnableFeature(
      features::kEnableFullscreenToAnyScreenAndroid);

  // Enables desktop page web prefs for large displays on Android.
  // TODO(crbug.com/433519850): Remove once feature is enabled by default.
  feature_overrides.EnableFeature(
      blink::features::kAndroidDesktopWebPrefsLargeDisplays);

  // Enable timeout for TextClassifier calls.
  // TODO(crbug.com/504722790): Remove when experiment is complete.
  feature_overrides.EnableFeature(features::kTextClassifierTimeout);

  // Enable graceful tab shutdown.
  // TODO(crbug.com/532514154): Remove when experiment is complete.
  feature_overrides.EnableFeature(chrome::android::kTabAndroidGracefulShutdown);

  // Enable desktop fling curve.
  feature_overrides.EnableFeature(features::kDesktopFlingCurveOnAndroid);

  // Disable modern overscroll animations and gestures on Desktop Android.
  feature_overrides.DisableFeature(features::kElasticOverscroll);
  feature_overrides.DisableFeature(features::kOverscrollHistoryNavigation);
  feature_overrides.DisableFeature(
      features::kOverscrollEffectOnNonRootScrollers);

  // Suppress fallback to the legacy Android edge glow shade on Desktop Android.
  feature_overrides.EnableFeature(features::kSuppressOverscrollGlow);

  // Enable Glic and side panel features on Desktop Android.
  // TODO(crbug.com/545760718): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(chrome::android::kEnableAndroidSidePanel);
  feature_overrides.EnableFeature(features::kGlic);
  feature_overrides.EnableFeature(features::kGlicActor);
  feature_overrides.EnableFeature(features::kGlicAndroidSidePanel);
  feature_overrides.EnableFeature(features::kGlicRollout);
  feature_overrides.EnableFeature(glic::kContextualCueing);

  // As of writing, the only devices that can make use of browsing history
  // donation are desktop devices.
  // TODO(crbug.com/546011402): Remove this heuristic once we can detect
  // whether the data consumer will actually use the data.
  feature_overrides.EnableFeature(
      chrome::android::kAuxiliarySearchHistoryDonation);

  // Allows IMEs to insert media content such as images, gifs and stickers on
  // Android Desktop devices.
  // TODO(crbug.com/404663565): Remove when rollout to all form factors is
  // complete.
  feature_overrides.EnableFeature(features::kAndroidMediaInsertion);

  // Enables Custom IME Spellcheck UI on Android Desktop devices.
  // TODO(crbug.com/553988342): Remove when rollout to all form factors is
  // complete.
  feature_overrides.EnableFeature(
      blink::features::kAndroidSpellcheckFullApiBlink);
  feature_overrides.EnableFeature(blink::features::kAndroidSpellcheckNativeUi);

  // Enable updated FRE layout on Android Desktop.
  // TODO(crbug.com/534451983): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(chrome::android::kAndroidFreLayoutUpdate);

  // Enable Account Picker dialog on Android Desktop.
  // TODO(crbug.com/553630105): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(chrome::android::kAccountPickerDialog);

  // Disables the Grid Tab Switcher (Hub layout) on Desktop Android in favor of
  // the desktop tab strip.
  // TODO(crbug.com/545634112): Remove when launched to 100% on Desktop Android.
  feature_overrides.EnableFeature(chrome::android::kDisableGridTabSwitcher);

  // Enables spoofing the user agent platform as ChromeOS on desktop Android.
  // TODO(crbug.com/556358275): Enablement on tablets is tracked by this bug.
  feature_overrides.EnableFeature(
      blink::features::kAndroidDesktopUASpoofAsChromeOS);

  // Enable opening PDFs in iframe in standalone tabs on Android.
  // TODO(crbug.com/556810751) Enable on non-AL form factors.
  feature_overrides.EnableFeature(blink::features::kAndroidHandlePdfInIframe);

#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
  // Desktop-first features which are past incubation should either end up here,
  // or to a finch trial that enables it for all form factors.
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeBrowserFieldTrials::EnableRuntimeMutableFeatures(
    base::FeatureList* feature_list) {
  // Add calls to enable runtime-mutable features here.
}
