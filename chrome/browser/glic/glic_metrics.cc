// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

namespace {

bool CheckFreStatus(GlicEnabling* enabling, prefs::FreStatus status) {
  return enabling->GetCompletedFre() == status;
}

std::string_view GetInputModeString(mojom::WebClientMode input_mode) {
  switch (input_mode) {
    case mojom::WebClientMode::kText:
      return "Text";
    case mojom::WebClientMode::kAudio:
      return "Audio";
    case mojom::WebClientMode::kUnknown:
      return "Unknown";
  }
}

}  // namespace

GlicMetrics::GlicMetrics(Profile* profile, GlicEnabling* enabling)
    : profile_(profile), enabling_(enabling) {
  impression_timer_.Start(
      FROM_HERE, base::Minutes(15),
      base::BindRepeating(&GlicMetrics::OnImpressionTimerFired,
                          base::Unretained(this)));

  subscriptions_.push_back(
      enabling_->RegisterAllowedChanged(base::BindRepeating(
          &GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged,
          base::Unretained(this))));

  if (enabling_->IsAllowed()) {
    RecordStartupEnablement();
  }

  is_enabled_ = enabling_->IsEnabledAndConsentForProfile(profile_);
  is_pinned_ = profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  pref_registrar_.Init(profile_->GetPrefs());
  subscriptions_.push_back(
      enabling_->RegisterOnConsentChanged(base::BindRepeating(
          &GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged,
          base::Unretained(this))));
  pref_registrar_.Add(prefs::kGlicPinnedToTabstrip,
                      base::BindRepeating(&GlicMetrics::OnPinningPrefChanged,
                                          base::Unretained(this)));
}

GlicMetrics::~GlicMetrics() = default;

void GlicMetrics::RecordGlicProfilePreferences() {
  PrefService* profile_prefs = profile_->GetPrefs();
  PrefService* local_state = g_browser_process->local_state();

  base::UmaHistogramBoolean(
      "Glic.Preferences.PinnedToTabstrip",
      profile_prefs->GetBoolean(prefs::kGlicPinnedToTabstrip));
  base::UmaHistogramBoolean(
      "Glic.Preferences.LauncherEnabled",
      local_state->GetBoolean(prefs::kGlicLauncherEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.KeepSidepanelOpenOnNewTabsEnabled",
      profile_prefs->GetBoolean(prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.GeolocationEnabled",
      profile_prefs->GetBoolean(prefs::kGlicGeolocationEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.MicrophoneEnabled",
      profile_prefs->GetBoolean(prefs::kGlicMicrophoneEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.DefaultTabContextEnabled",
      profile_prefs->GetBoolean(prefs::kGlicDefaultTabContextEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.ShakeTriggerEnabled",
      profile_prefs->GetBoolean(prefs::kGlicShakeTriggerEnabled));
  base::UmaHistogramBoolean("Glic.Preferences.ActuationOnWeb",
                            enabling_->GetUserEnabledActuationOnWeb());

#if !BUILDFLAG(IS_ANDROID)
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (settings_map) {
    base::UmaHistogramBoolean("Glic.Selection.InlineCueMenuEnabled",
                              settings_map->GetDefaultContentSetting(
                                  ContentSettingsType::INLINE_CUE_MENU,
                                  nullptr) == CONTENT_SETTING_ALLOW);

    ContentSettingsForOneType exceptions = settings_map->GetSettingsForOneType(
        ContentSettingsType::INLINE_CUE_MENU);
    size_t site_exceptions_count = 0;
    size_t removed_default_blocked_sites_count = 0;

    for (const auto& exception : exceptions) {
      if (exception.primary_pattern.MatchesAllHosts()) {
        continue;
      }
      if (exception.GetContentSetting() == CONTENT_SETTING_BLOCK) {
        site_exceptions_count++;
      } else if (exception.GetContentSetting() == CONTENT_SETTING_ALLOW) {
        // Explicit ALLOW exceptions for the inline cue menu are only created
        // when a user unblocks a default-blocked site in Settings.
        removed_default_blocked_sites_count++;
      }
    }
    base::UmaHistogramCounts100("Glic.Selection.SiteExceptionsCount",
                                site_exceptions_count);
    base::UmaHistogramCounts100(
        "Glic.Selection.RemovedDefaultBlockedSitesCount",
        removed_default_blocked_sites_count);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void GlicMetrics::OnTrustFirstOnboardingAccept() {
  OnFreAccepted();
  OnOptInAccepted(OptInFlow::kGlicFre);
  base::UmaHistogramEnumeration("Glic.Fre.Accept.InvocationSource",
                                invocation_source_);

  if (!onboarding_shown_time_.is_null()) {
    base::UmaHistogramLongTimes(
        "Glic.Fre.TotalTime.Accepted.Onboarding",
        base::TimeTicks::Now() - onboarding_shown_time_);
    onboarding_shown_time_ = base::TimeTicks();
  }
}

void GlicMetrics::OnInstanceOpened() {
  if (!onboarding_shown_time_.is_null()) {
    return;
  }

  if (!enabling_->HasConsented()) {
    OnOptInShown(OptInFlow::kGlicFre);
    base::UmaHistogramEnumeration("Glic.Fre.Shown.InvocationSource",
                                  invocation_source_);
    onboarding_shown_time_ = base::TimeTicks::Now();
  }
}

void GlicMetrics::OnInstanceClosed() {
  if (onboarding_shown_time_.is_null()) {
    return;
  }

  OnOptInDismissed(OptInFlow::kGlicFre);
  base::UmaHistogramEnumeration("Glic.Fre.Dismissed.InvocationSource",
                                invocation_source_);
  base::UmaHistogramLongTimes("Glic.Fre.TotalTime.Dismissed.Onboarding",
                              base::TimeTicks::Now() - onboarding_shown_time_);
  onboarding_shown_time_ = base::TimeTicks();
}

void GlicMetrics::OnFreAccepted() {
  // Store the current time in a instance variable.
  fre_accepted_time_ = base::TimeTicks::Now();
  onboarding_invocation_source_ = invocation_source_;
}

void GlicMetrics::OnOptInShown(OptInFlow flow) {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Shown"));
  base::UmaHistogramEnumeration("Glic.Fre.Shown.FlowSource", flow);
}

void GlicMetrics::OnOptInImpression(OptInFlow flow) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Onboarding.OptInImpression"));
  base::UmaHistogramEnumeration("Glic.Onboarding.OptInImpression.FlowSource",
                                flow);
}

void GlicMetrics::OnOptInAccepted(OptInFlow flow) {
  base::RecordAction(base::UserMetricsAction("Glic.Onboarding.OptInAccept"));
  base::UmaHistogramEnumeration("Glic.Fre.Accept.FlowSource", flow);
}

void GlicMetrics::OnOptInDismissed(OptInFlow flow) {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Dismissed"));
  base::UmaHistogramEnumeration("Glic.Fre.Dismissed.FlowSource", flow);
}

void GlicMetrics::OnOptInRejected(OptInFlow flow) {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.NoThanks"));
  base::UmaHistogramEnumeration("Glic.Fre.NoThanks.FlowSource", flow);
}

void GlicMetrics::OnUserInputSubmitted() {
  if (!fre_accepted_time_.is_null()) {
    base::TimeDelta delta = base::TimeTicks::Now() - fre_accepted_time_;
    base::RecordAction(base::UserMetricsAction("Glic.Fre.InputSubmitted"));
    base::UmaHistogramLongTimes("Glic.FreToFirstQueryTime", delta);
    base::UmaHistogramCustomTimes("Glic.FreToFirstQueryTimeMax24H", delta,
                                  base::Milliseconds(1), base::Hours(24), 50);
    base::UmaHistogramEnumeration("Glic.Fre.UserInput.InvocationSource",
                                  onboarding_invocation_source_);
    fre_accepted_time_ = base::TimeTicks();
  }
}

void GlicMetrics::OnContextUploadStarted() {
  last_upload_start_time_ = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("GlicContextUploadStarted"));
}

void GlicMetrics::OnContextUploadCompleted() {
  if (last_upload_start_time_) {
    base::UmaHistogramMediumTimes(
        "Glic.TabContext.UploadTime",
        base::TimeTicks::Now() - *last_upload_start_time_);
    last_upload_start_time_ = std::nullopt;
  }
  base::RecordAction(base::UserMetricsAction("GlicContextUploadCompleted"));
}

void GlicMetrics::OnSessionTerminated() {
  base::RecordAction(base::UserMetricsAction("GlicWebClientSessionEnd"));
}

void GlicMetrics::OnResponseRated(bool positive) {
  base::UmaHistogramBoolean("Glic.Response.Rated", positive);
}

void GlicMetrics::OnGlicWindowStartedOpening(bool attached,
                                             mojom::InvocationSource source) {
  // Sets `invocation_source_`, triggers web client FRE metrics via
  // OnInstanceOpened(), and records the Glic.WindowOpen UKM and
  // Glic.PanelWebUi.ElapsedTimeBetweenSessions.
  invocation_source_ = source;

  // This method depends on first setting invocation_source_. This is used for
  // web client FRE metrics.
  OnInstanceOpened();

  // TODO(b/452120577): No tab context is known at this point, so no page
  // source ID is attached.
  ukm::builders::Glic_WindowOpen(ukm::NoURLSourceId())
      .SetAttached(attached)
      .SetInvocationSource(static_cast<int64_t>(source))
      .Record(ukm::UkmRecorder::Get());

  const base::Time last_dismissed_time =
      profile_->GetPrefs()->GetTime(prefs::kGlicWindowLastDismissedTime);
  if (!last_dismissed_time.is_null()) {
    base::TimeDelta elapsed_time_from_last_session =
        base::Time::Now() - last_dismissed_time;
    base::UmaHistogramCounts10M(
        "Glic.PanelWebUi.ElapsedTimeBetweenSessions",
        base::saturated_cast<int>(elapsed_time_from_last_session.InSeconds()));
  }

  // Update the last dismissed timestamp. The pref might not get updated on
  // ungraceful shutdowns. As such, by updating the pref on opening the Glic
  // window, the dismissal timestamp will get approximated by the opening
  // timestamp, instead of the previously dismissal timestamp.
  profile_->GetPrefs()->SetTime(prefs::kGlicWindowLastDismissedTime,
                                base::Time::Now());
}

void GlicMetrics::LogClosedCaptionsShown() {
  bool pref_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicClosedCaptioningEnabled);
  base::UmaHistogramBoolean("Glic.Response.ClosedCaptionsShown", pref_enabled);
}

void GlicMetrics::OnShareImageStarted() {
  share_image_start_time_ = base::TimeTicks::Now();
}

void GlicMetrics::OnShareImageComplete(ShareImageResult result) {
  if (!share_image_start_time_.is_null() &&
      result == ShareImageResult::kSentImageToClient) {
    base::UmaHistogramMediumTimes(
        "Glic.TabContext.ShareImageDuration",
        base::TimeTicks::Now() - share_image_start_time_);
    share_image_start_time_ = base::TimeTicks();
  }
  base::UmaHistogramEnumeration("Glic.TabContext.ShareImageResult", result);
}

void GlicMetrics::LogGetContextFromFocusedTabError(
    GlicGetContextFromTabError error) {
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Api.GetContextFromFocusedTab.Error.", mode_string}),
      error);
}

void GlicMetrics::LogGetContextFromTabError(GlicGetContextFromTabError error) {
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Api.GetContextFromTab.Error.", mode_string}), error);
}

void GlicMetrics::LogGetContextForActorFromTabError(
    GlicGetContextFromTabError error) {
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Api.GetContextForActorFromTab.Error.", mode_string}),
      error);
}

void GlicMetrics::LogGetImageBytesFromTabError(
    GlicGetContextFromTabError error) {
  base::UmaHistogramEnumeration("Glic.Api.GetImageBytesFromTab.Error", error);
}

void GlicMetrics::OnActivateTabFromInstance(tabs::TabInterface* tab) {
  const actor::ActorTask* task =
      actor::ActorKeyedService::Get(profile_)->GetTaskFromTab(*tab);
  // Record user action if the tab is associated with an ActorTask.
  if (task) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.TaskTabForegrounded"));
  }
}

void GlicMetrics::SetWebClientMode(mojom::WebClientMode mode) {
  input_mode_ = mode;
}

void GlicMetrics::OnImpressionTimerFired() {
  if (!enabling_->IsAllowed()) {
    EntryPointStatus impression;
    if (CheckFreStatus(enabling_, prefs::FreStatus::kNotStarted)) {
      // Profile not eligible, and not started FRE
      impression = EntryPointStatus::kBeforeFreNotEligible;
    } else if (CheckFreStatus(enabling_, prefs::FreStatus::kIncomplete)) {
      // Profile not eligible, started but not completed FRE
      impression = EntryPointStatus::kIncompleteFreNotEligible;
    } else {
      // Profile not eligible, completed FRE
      impression = EntryPointStatus::kAfterFreNotEligible;
    }
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status", impression);
    return;
  }

  // Profile eligible, has not started FRE
  if (CheckFreStatus(enabling_, prefs::FreStatus::kNotStarted)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kBeforeFreAndEligible);
    return;
  }

  // Profile eligible, started but not completed FRE
  if (CheckFreStatus(enabling_, prefs::FreStatus::kIncomplete)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kIncompleteFreAndEligible);
    return;
  }

  // Profile eligible and completed FRE
  EntryPointStatus impression;
  auto enablement = GlicEnabling::EnablementForProfile(profile_);
  if (enablement.anchor_entrypoint_override_active) {
    impression = EntryPointStatus::kAfterFreAnchoredButIneligible;
  } else {
#if BUILDFLAG(IS_ANDROID)
    bool is_bottom_bar_enabled = false;
    bool is_mtb_enabled = false;
    GlicKeyedService* service = GlicKeyedService::Get(profile_);
    if (service) {
      is_bottom_bar_enabled = service->IsBottomBarEnabled();
      if (!is_bottom_bar_enabled) {
        is_mtb_enabled = service->IsGlicShortcutActive();
      }
    }
    if (is_mtb_enabled || is_bottom_bar_enabled) {
      impression = EntryPointStatus::kAfterFreBrowserOnly;
    } else {
      impression = EntryPointStatus::kAfterFreThreeDotOnly;
    }
#else
    bool is_os_entrypoint_enabled =
        g_browser_process->local_state()->GetBoolean(
            prefs::kGlicLauncherEnabled);
    if (is_pinned_ && is_os_entrypoint_enabled) {
      impression = EntryPointStatus::kAfterFreBrowserAndOs;
    } else if (is_pinned_) {
      impression = EntryPointStatus::kAfterFreBrowserOnly;
    } else if (is_os_entrypoint_enabled) {
      impression = EntryPointStatus::kAfterFreOsOnly;
    } else {
      impression = EntryPointStatus::kAfterFreThreeDotOnly;
    }
#endif
  }
  // TODO(crbug.com/520136927): Move this metric to glic_metrics_provider.cc
  // when glic_metrics.cc is deleted.
  base::UmaHistogramEnumeration("Glic.EntryPoint.Status", impression);

#if !BUILDFLAG(IS_ANDROID)
  ui::Accelerator saved_hotkey =
      glic::GlicLauncherConfiguration::GetToggleHotkey();
  base::UmaHistogramBoolean("Glic.OsEntrypoint.Settings.ShortcutStatus",
                            saved_hotkey != ui::Accelerator());
#endif
}

void GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged() {
  if (!recorded_startup_enablement_ && enabling_->IsAllowed()) {
    RecordStartupEnablement();
  }

  bool is_enabled = enabling_->IsEnabledAndConsentForProfile(profile_);
  if (is_enabled == is_enabled_) {
    // No change, early exit.
    return;
  }
  is_enabled_ = is_enabled;
  if (!is_enabled_) {
    base::RecordAction(base::UserMetricsAction("Glic.Disabled"));
  }
}

void GlicMetrics::OnPinningPrefChanged() {
  bool is_pinned =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  if (is_pinned == is_pinned_) {
    // No change, early exit.
    return;
  }
  is_pinned_ = is_pinned;
  if (is_pinned_) {
    base::RecordAction(base::UserMetricsAction("Glic.Pinned"));
  } else {
    base::RecordAction(base::UserMetricsAction("Glic.Unpinned"));
  }
}

void GlicMetrics::OnTabPinnedForSharing(GlicTabPinnedForSharingResult result) {
  base::UmaHistogramEnumeration("Glic.Sharing.TabPinnedForSharing", result);
}

void GlicMetrics::RecordStartupEnablement() {
  base::TimeTicks startup_time =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  if (startup_time.is_null()) {
    return;
  }

  base::TimeDelta delta = base::TimeTicks::Now() - startup_time;
  base::UmaHistogramLongTimes("Glic.ProfileEnablement.TimeToEnabledFromStartup",
                              delta);
  recorded_startup_enablement_ = true;
}

void RecordHotkeyCleared(prefs::DefaultHotkeyScope default_scope,
                         bool is_global,
                         bool launcher_enabled) {
  if (default_scope == prefs::DefaultHotkeyScope::kGlobal) {
    base::UmaHistogramEnumeration(
        "Glic.Preferences.HotkeyCleared",
        is_global ? GlicHotkeyCleared::kDefaultGlobalWithGlobalScope
                  : GlicHotkeyCleared::kDefaultGlobalWithLocalScope);
  } else if (default_scope == prefs::DefaultHotkeyScope::kLocal) {
    base::UmaHistogramEnumeration(
        "Glic.Preferences.HotkeyCleared",
        is_global ? GlicHotkeyCleared::kDefaultLocalWithGlobalScope
                  : GlicHotkeyCleared::kDefaultLocalWithLocalScope);
  } else if (launcher_enabled) {
    // When `default_scope` is `kNotMigrated` (i.e. `kGlicHotkeyLocalScope` is
    // disabled) and the launcher is enabled, the user has an active global
    // hotkey and maps to `kDefaultGlobalWithGlobalScope` for Control vs.
    // Treatment comparison.
    base::UmaHistogramEnumeration(
        "Glic.Preferences.HotkeyCleared",
        GlicHotkeyCleared::kDefaultGlobalWithGlobalScope);
  }
}

void RecordHotkeyScopeChange(GlicHotkeyScopeChange scope_change) {
  base::UmaHistogramEnumeration("Glic.Preferences.HotkeyScopeChange",
                                scope_change);
}

}  // namespace glic
