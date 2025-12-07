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
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/feed/feed_feature_list.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_histograms.h"
#include "components/variations/feature_overrides.h"
#include "components/version_info/version_info.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/background_thread_pool_field_trial.h"
#include "base/android/bundle_utils.h"
#include "base/task/thread_pool/environment_config.h"
#include "build/android_buildflags.h"
#include "chrome/browser/android/flags/chrome_cached_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "sandbox/policy/features.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/channel_info.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/first_run_field_trial.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_features.h"
#endif  // BUILDFLAG(IS_LINUX)

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

#if BUILDFLAG(IS_LINUX)
  // On Linux/Desktop platform variants, such as ozone/wayland, some features
  // might need to be disabled as per OzonePlatform's runtime properties.
  // OzonePlatform selection and initialization, in turn, depend on Chrome flags
  // processing, namely 'ozone-platform-hint', so do it here.
  //
  // TODO(nickdiego): Move it back to
  // ChromeMainDelegate::PostEarlyInitialization.

  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string xdg_session_type =
      env->GetVar(base::nix::kXdgSessionTypeEnvVar).value_or(std::string());

  if (xdg_session_type == "wayland") {
    feature_overrides.DisableFeature(features::kEyeDropper);
  }
#elif BUILDFLAG(IS_ANDROID)  // BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // Nota bene: Anything here is expected to be short-lived, unless deemed too
  // risky to launch to non-desktop platforms. New features being added here
  // should be the exception, and not the norm. Instead, you should place the
  // override in the generic IS_ANDROID block below, guarded by an appropriate
  // runtime check.

  // If enabled, then use desktop page webprefs for Android devices that have
  // large displays, specifically tablets and desktops.
  feature_overrides.EnableFeature(
      blink::features::kAndroidDesktopWebPrefsLargeDisplays);

  // Enables the caret browsing a11y feature - can use arrow keys to navigate
  // through web pages.
  // TODO(crbug.com/369139090): Remove when rollout is complete
  feature_overrides.EnableFeature(features::kAndroidCaretBrowsing);

  // Enable the link hover status bar.
  // TODO(crbug.com/404678510): Remove when the feature is stable.
  feature_overrides.EnableFeature(chrome::android::kLinkHoverStatusBar);

  // If enabled, render processes associated only with tabs in unfocused windows
  // will be downgraded to "vis" priority, rather than remaining at "fg". This
  // will allow tabs in unfocused windows to be prioritized for OOM kill in
  // low-memory scenarios.
  feature_overrides.EnableFeature(chrome::android::kChangeUnfocusedPriority);

  // Enables media capture (tab+window+screen sharing).
  // TODO(crbug.com/352187279): Remove when tablet rollout is complete.
  feature_overrides.EnableFeature(kAndroidMediaPicker);
  feature_overrides.EnableFeature(features::kUserMediaScreenCapturing);

  // Enable desktop tab management features.
  // TODO(crbug.com/422902625): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(chrome::android::kProcessRankPolicyAndroid);
  feature_overrides.EnableFeature(chrome::android::kProtectedTabsAndroid);
  feature_overrides.EnableFeature(features::kSubframeImportance);
  // TODO(crbug.com/422903297): Remove when tablet rollout is complete.
  feature_overrides.EnableFeature(features::kRendererProcessLimitOnAndroid);
  // Enable V8 optimizations for high-end Android Desktop devices.
  // TODO(crbug.com/425860368): Remove when the feature is stable.
  feature_overrides.EnableFeature(features::kV8AndroidDesktopHighEndConfig);
  // TODO(b/432367402): Use a new Android API to replace this hack with a proper
  // solution.
  feature_overrides.EnableFeature(features::kAndroidCaptureKeyEvents);
  // TODO(crbug.com/438369690): Remove when we enable DevTools frontend for all
  // clank users.
  feature_overrides.EnableFeature(features::kAndroidDevToolsFrontend);
  // TODO(crbug.com/430304112): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(
      autofill::features::kAutofillAndroidDesktopSuppressAccessoryOnEmpty);
  // TODO(crbug.com/436900619): Remove when the long term solution is
  // implemented.
  feature_overrides.EnableFeature(
      chrome::android::kLockTopControlsOnLargeTablets);
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
  // Disables the enhanced pip transition and uses the default animation.
  // TODO(crbug.com/440384447): Remove when enhanced pip transition is fixed.
  feature_overrides.DisableFeature(media::kAllowEnhancedPipTransition);
  // Enable by default for desktop platforms, pending a phone / foldable /
  // tablet rollout using the same flag.
  // TODO(crbug.com/413776899): Remove when rollout on other form factors is
  // complete.
  feature_overrides.EnableFeature(chrome::android::kInstanceSwitcherV2);
  // TODO(crbug.com/442327273): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(
      autofill::features::kAutofillAndroidDesktopKeyboardAccessoryRevamp);
  // TODO(crbug.com/444486763): Remove when rollout is complete to all form
  // factors.
  feature_overrides.EnableFeature(chrome::android::kAndroidTabHighlighting);
  // TODO(b/441672693): Remove when the feature is stable on other form factors.
  feature_overrides.EnableFeature(features::kAndroidAudioDeviceListener);
  // Enable by default for desktop platforms, pending a tablet rollout using the
  // same flag.
  // TODO(crbug.com/445475304): Remove when tablet rollout is complete.
  feature_overrides.EnableFeature(feed::kAndroidOpenIncognitoAsWindow);
  feature_overrides.EnableFeature(chrome::android::kTabStripIncognitoMigration);
  // TODO(crbug.com/427242080): Remove when tablet rollout is complete.
  feature_overrides.EnableFeature(
      chrome::android::kAndroidPinnedTabsTabletTabStrip);

  // Three flags are required for the bookmarks bar feature.
  // TODO(crbug.com/430059235): Remove once feature is launched to 100% on all
  // form factors.
  feature_overrides.EnableFeature(chrome::android::kAndroidBookmarkBar);
  feature_overrides.EnableFeature(chrome::android::kAndroidAppearanceSettings);
  feature_overrides.EnableFeature(chrome::android::kTopControlsRefactor);

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

  // Enable all tabs to have WebContents at all times for desktop platforms.
  // TODO(crbug.com/448420873): Remove once we enable this feature for all form
  // factors. This is currently blocked by performance regressions on low-end
  // Android devices.
  feature_overrides.EnableFeature(features::kWebContentsDiscard);
  feature_overrides.EnableFeature(features::kLazyBrowserInterfaceBroker);
  feature_overrides.EnableFeature(chrome::android::kTabFreezingUsesDiscard);
  feature_overrides.EnableFeature(chrome::android::kLoadAllTabsAtStartup);

  // Enable the ability for extensions to override chrome pages.
  // TODO(crbug.com/404069963): Remove flag when the feature is verified to be
  // stable on desktop Android.
  feature_overrides.EnableFeature(chrome::android::kChromeNativeUrlOverriding);

  // Enable desktop full screen feature flags by default for desktop platforms.
  // This includes: Display Edge to Edge fullscreen and full screen to any
  // screen
  // TODO(crbug.com/417426218) Remove once feature is launched to 100% on all
  // form factors.
  feature_overrides.EnableFeature(features::kDisplayEdgeToEdgeFullscreen);
  feature_overrides.EnableFeature(
      features::kEnableFullscreenToAnyScreenAndroid);

  // Enables the ability to specify a platform-specific zoom scaling that will
  // apply transparently to all pages.
  // TODO(crbug.com/450281745): Remove once feature is enabled by default.
  feature_overrides.EnableFeature(::features::kAndroidDesktopZoomScaling);
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
  // Desktop-first features which are past incubation should either end up here,
  // or to a finch trial that enables it for all form factors.
#endif  // BUILDFLAG(IS_ANDROID)
}
