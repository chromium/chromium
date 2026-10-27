// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_fre_impression_cui_tracker.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/public/glic_response_completion_cui_tracker.h"
#include "chrome/browser/glic/public/glic_submit_query_cui_tracker.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_state_tracker.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/metrics/structured/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/skills/public/skills_metrics.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#endif

namespace glic {

// User actions are not split by provider; histograms carry the provider split.

namespace {

SafeEmbedderKey ToSafeKey(const EmbedderKey& key) {
  return std::visit(
      absl::Overload{[](const SidePanelEmbedderKey& key) -> SafeEmbedderKey {
                       return SafeEmbedderKey(key.tab->GetHandle());
                     },
                     [](const FloatingEmbedderKey& key) -> SafeEmbedderKey {
                       return SafeEmbedderKey(key);
                     }},
      key);
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

EmbedderType GetEmbedderTypeFromShowOptions(const ShowOptions& options) {
  if (std::holds_alternative<SidePanelShowOptions>(options.embedder_options)) {
    return EmbedderType::kSidePanel;
  }
  if (std::holds_alternative<FloatingShowOptions>(options.embedder_options)) {
    return EmbedderType::kFloaty;
  }
  return EmbedderType::kUnknown;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicTurnSource)
enum class GlicTurnSource {
  kUnknown = 0,
  kSidePanelText = 1,
  kSidePanelAudio = 2,
  kFloatyText = 3,
  kFloatyAudio = 4,
  kTabText = 5,
  kTabAudio = 6,
  kMaxValue = kTabAudio,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicTurnSource)

void RecordSelectionOverlayMetrics(
    const std::vector<raw_ptr<tabs::TabInterface>>& pinned_tabs,
    const GlicMetricReporter& reporter) {
// Selection Overlays are not currently implemented on Android.
#if !BUILDFLAG(IS_ANDROID)
  int selection_areas_count = 0;
  std::vector<int> polyline_point_counts;
  for (tabs::TabInterface* tab : pinned_tabs) {
    if (auto* web_contents = tab->GetContents()) {
      if (auto* selection_overlay_controller =
              SelectionOverlayController::FromTabWebContents(web_contents)) {
        selection_areas_count +=
            selection_overlay_controller->GetSelectedRegionCount();
        std::vector<int> counts =
            selection_overlay_controller->GetPolylineCounts();
        polyline_point_counts.insert(polyline_point_counts.end(),
                                     counts.begin(), counts.end());
      }
    }
  }
  if (base::FeatureList::IsEnabled(features::kGlicCaptureRegion)) {
    reporter.RecordExactLinear("Instance.InputSubmitted.SelectionCount",
                               selection_areas_count, 10);
  }
  if (base::FeatureList::IsEnabled(features::kGlicRegionSelectionLine)) {
    for (int count : polyline_point_counts) {
      reporter.RecordCounts1000(
          "Instance.InputSubmitted.Selection.PolylinePointCount", count);
    }
  }
#endif
}

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
// Translates the browser-side enum into the structured metrics enum generated
// from structured.xml. Written as an exhaustive switch with no default so that
// adding a ClientLoadErrorReason without adding the matching structured.xml
// variant is a compile error rather than a silently mislabelled metric.
metrics::structured::events::v2::glic::GlicClientLoadErrorReason
ToStructuredReason(ClientLoadErrorReason reason) {
  using Reason =
      metrics::structured::events::v2::glic::GlicClientLoadErrorReason;
  switch (reason) {
    case ClientLoadErrorReason::kUnknown:
      return Reason::UNKNOWN;
    case ClientLoadErrorReason::kUnavailable:
      return Reason::UNAVAILABLE;
    case ClientLoadErrorReason::kIneligibleAccount:
      return Reason::INELIGIBLE_ACCOUNT;
    case ClientLoadErrorReason::kLocationMismatch:
      return Reason::LOCATION_MISMATCH;
    case ClientLoadErrorReason::kDisabledByAdmin:
      return Reason::DISABLED_BY_ADMIN;
    case ClientLoadErrorReason::kOffline:
      return Reason::OFFLINE;
    case ClientLoadErrorReason::kSignIn:
      return Reason::SIGN_IN;
    case ClientLoadErrorReason::kCookieSyncFailed:
      return Reason::COOKIE_SYNC_FAILED;
    case ClientLoadErrorReason::kGuestLoadFailed:
      return Reason::GUEST_LOAD_FAILED;
    case ClientLoadErrorReason::kGuestProcessGone:
      return Reason::GUEST_PROCESS_GONE;
    case ClientLoadErrorReason::kClientError:
      return Reason::CLIENT_ERROR;
    case ClientLoadErrorReason::kClientLoadTimeout:
      return Reason::CLIENT_LOAD_TIMEOUT;
    case ClientLoadErrorReason::kWarmedTimeout:
      return Reason::WARMED_TIMEOUT;
    case ClientLoadErrorReason::kGuestOriginBlocked:
      return Reason::GUEST_ORIGIN_BLOCKED;
    case ClientLoadErrorReason::kGuestApiNotAllowed:
      return Reason::GUEST_API_NOT_ALLOWED;
  }
}
#endif

}  // namespace

GlicInstanceMetrics::TurnInfo::TurnInfo() = default;
GlicInstanceMetrics::TurnInfo::~TurnInfo() = default;

GlicInstanceMetrics::GlicInstanceMetrics(
    const metrics::ProfileMetricsService* profile_metrics_service,
    Profile* profile)
    : creation_time_(base::TimeTicks::Now()),
      metric_reporter_(profile),
      session_manager_(this, metric_reporter_),
      profile_metrics_service_(CHECK_DEREF(profile_metrics_service)),
      saas_usage_reporting_controller_(nullptr),
      profile_(profile),
      pref_service_(profile ? profile->GetPrefs() : nullptr) {
  CHECK(profile);
  // Used in the unit tests.
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Created"));
  // Not split: recorded by GlicStateTracker, not this class.
  activity_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedActiveDuration");
  visibility_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedVisibleDuration");
  LogEvent(GlicInstanceEvent::kInstanceCreated);
}

GlicInstanceMetrics::GlicInstanceMetrics(
    const metrics::ProfileMetricsService* profile_metrics_service,
    GlicSharingManagerInternal* sharing_manager,
    enterprise_reporting::SaasUsageReportingController*
        saas_usage_reporting_controller,
    Profile* profile)
    : creation_time_(base::TimeTicks::Now()),
      metric_reporter_(profile),
      session_manager_(this, metric_reporter_),
      pinned_tabs_changed_subscription_(
          sharing_manager->AddPinnedTabsChangedCallback(
              base::BindRepeating(&GlicInstanceMetrics::OnPinnedTabsChanged,
                                  base::Unretained(this)))),
      tab_pinning_status_subscription_(
          sharing_manager->AddTabPinningStatusEventCallback(base::BindRepeating(
              &GlicInstanceMetrics::RecordTabPinningStatusEvent,
              base::Unretained(this)))),
      profile_metrics_service_(CHECK_DEREF(profile_metrics_service)),
      sharing_manager_(sharing_manager),
      saas_usage_reporting_controller_(saas_usage_reporting_controller),
      profile_(profile),
      pref_service_(profile ? profile->GetPrefs() : nullptr) {
  CHECK(profile);
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Created"));
  // Not split: recorded by GlicStateTracker, not this class.
  activity_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedActiveDuration");
  visibility_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedVisibleDuration");
  LogEvent(GlicInstanceEvent::kInstanceCreated);
}

GlicInstanceMetrics::~GlicInstanceMetrics() {
  OnInstanceDestroyed();
}

void GlicInstanceMetrics::OnOptinImpression() {
  if (!base::FeatureList::IsEnabled(features::kGlicOptInImpressionMetrics)) {
    return;
  }
  is_opt_in_pending_ = true;
  MaybeRecordOptInImpression();
}

void GlicInstanceMetrics::MaybeRecordOptInImpression() {
  if (!is_opt_in_pending_ ||
      !base::FeatureList::IsEnabled(features::kGlicOptInImpressionMetrics) ||
      !is_client_ready_ || !visibility_tracker_->state()) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("Glic.Onboarding.OptInImpression"));
  // Not split: consumer FRE/opt-in only; GEiC FRE tracked in b/571507463.
  base::UmaHistogramEnumeration("Glic.Onboarding.OptInImpression.FlowSource",
                                OptInFlow::kGlicFre);
  if (opt_in_shown_callback_) {
    tabs::TabInterface* tab =
        sharing_manager_ ? sharing_manager_->GetFocusedTabData().focus()
                         : nullptr;
    opt_in_shown_callback_.Run(GetUkmSourceIdForTab(tab));
  }
  is_opt_in_pending_ = false;
  opt_in_impression_recorded_for_current_open_ = true;
  LogEvent(GlicInstanceEvent::kOptInImpression);
}

void GlicInstanceMetrics::OnGlicScrollAttempt() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  ++scroll_attempt_count_;
  turn_.pending_scroll_complete_ = true;
}

void GlicInstanceMetrics::OnGlicScrollComplete(bool success) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  auto record_scroll_metric = [&](TurnInfo& turn_info) {
    if (success && !turn_info.input_submitted_time_.is_null()) {
      base::TimeDelta time_to_scroll =
          base::TimeTicks::Now() - turn_info.input_submitted_time_;
      std::string_view mode_string = GetInputModeString(turn_info.input_mode_);
      metric_reporter_.RecordMediumTimes(
          base::StrCat({"ScrollTo.UserPromptToScrollTime.", mode_string}),
          time_to_scroll);
    }
    turn_info.pending_scroll_complete_ = false;
  };

  if (last_turn_.pending_scroll_complete_) {
    record_scroll_metric(last_turn_);
  } else if (turn_.pending_scroll_complete_) {
    record_scroll_metric(turn_);
  }
}

void GlicInstanceMetrics::OnPinnedTabsChanged(
    const std::vector<raw_ptr<tabs::TabInterface>>& pinned_tabs) {
  pinned_tab_count_ = pinned_tabs.size();
  session_manager_.SetPinnedTabCount(pinned_tab_count_);
}

void GlicInstanceMetrics::RecordTabPinningStatusEvent(
    tabs::TabInterface* tab,
    GlicPinningStatusEvent event) {
  if (const auto* pin_event = std::get_if<GlicPinEvent>(&event)) {
    metric_reporter_.RecordEnumeration("Instance.TabPinTrigger",
                                       pin_event->trigger);
  } else if (const auto* unpin_event = std::get_if<GlicUnpinEvent>(&event)) {
    metric_reporter_.RecordEnumeration("Instance.TabUnpinTrigger",
                                       unpin_event->trigger);
  }
}

void GlicInstanceMetrics::OnInstanceDestroyed() {
  session_manager_.OnOwnerDestroyed();

  base::RecordAction(base::UserMetricsAction("Glic.Instance.Destroyed"));

  // Add the time spent in the final state before destruction.
  activity_tracker_->Finalize();
  visibility_tracker_->Finalize();

  const base::TimeDelta lifetime = base::TimeTicks::Now() - creation_time_;
  const base::TimeDelta total_active_time = activity_tracker_->total_duration();
  const base::TimeDelta total_visible_time =
      visibility_tracker_->total_duration();

  const base::TimeDelta background_time = lifetime - total_active_time;
  const base::TimeDelta hidden_time = lifetime - total_visible_time;

  metric_reporter_.RecordCustomTimes("Instance.TotalActiveDuration",
                                     total_active_time, base::Milliseconds(1),
                                     base::Hours(24), 50);
  metric_reporter_.RecordCustomTimes("Instance.TotalBackgroundDuration",
                                     background_time, base::Milliseconds(1),
                                     base::Hours(24), 50);
  metric_reporter_.RecordCustomTimes("Instance.TotalVisibleDuration",
                                     total_visible_time, base::Milliseconds(1),
                                     base::Hours(24), 50);
  metric_reporter_.RecordCustomTimes("Instance.TotalHiddenDuration",
                                     hidden_time, base::Milliseconds(1),
                                     base::Hours(24), 50);

  metric_reporter_.RecordCounts100("Instance.TotalTabsBoundInLifetime",
                                   GetEventCount(GlicInstanceEvent::kTabBound));
  metric_reporter_.RecordCounts100("Instance.MaxConcurrentlyBoundTabs",
                                   max_concurrently_bound_tabs_);
  // Not split: ProfileMetricsService adds its own {Profile} token.
  profile_metrics_service_->UmaHistogramCounts100(
      "Glic.Instance.TurnCount",
      GetEventCount(GlicInstanceEvent::kTurnCompleted));
  metric_reporter_.RecordCounts100("Instance.SessionCount", session_count_);

  InputModesUsed modes_used = InputModesUsed::kNone;
  bool has_audio = inputs_modes_used_.Has(mojom::WebClientMode::kAudio);
  bool has_text = inputs_modes_used_.Has(mojom::WebClientMode::kText);
  if (has_audio && has_text) {
    modes_used = InputModesUsed::kTextAndAudio;
  } else if (has_audio) {
    modes_used = InputModesUsed::kOnlyAudio;
  } else if (has_text) {
    modes_used = InputModesUsed::kOnlyText;
  }
  metric_reporter_.RecordEnumeration("Instance.InputModesUsed", modes_used);

  metric_reporter_.RecordCounts100("Instance.ZoomChangeCount",
                                   zoom_change_count_);
  zoom_change_count_ = 0;
}

void GlicInstanceMetrics::OnActivationChanged(bool is_active) {
  if (is_active_ == is_active) {
    return;
  }
  is_active_ = is_active;

  if (is_active) {
    // If this is not the first activation, log the time since the last one.
    if (!last_active_time_.is_null()) {
      base::TimeDelta time_since_last_active =
          base::TimeTicks::Now() - last_active_time_;
      metric_reporter_.RecordLongTimes("Instance.TimeSinceLastActive",
                                       time_since_last_active);
      metric_reporter_.RecordCustomTimes("Instance.TimeSinceLastActive.24H",
                                         time_since_last_active,
                                         base::Seconds(1), base::Hours(24), 50);
      metric_reporter_.RecordCustomTimes("Instance.TimeSinceLastActive.7D",
                                         time_since_last_active,
                                         base::Seconds(1), base::Days(7), 50);
    }
  }

  last_active_time_ = base::TimeTicks::Now();
  session_manager_.OnActivationChanged(is_active);
  activity_tracker_->OnStateChanged(is_active);
}

void GlicInstanceMetrics::OnVisibilityChanged(bool is_visible) {
  session_manager_.OnVisibilityChanged(is_visible);
  const bool was_visible = visibility_tracker_->state();
  visibility_tracker_->OnStateChanged(is_visible);
  if (was_visible && !is_visible) {
    OnInstanceHidden();
  }
  if (!was_visible && is_visible &&
      guest_actionable_.frame_submitted_while_hidden &&
      !actionable_invocation_state_.start_time.is_null()) {
    // The guest's frame was ready while the panel was briefly hidden; it's on
    // screen as soon as the panel is shown again.
    RecordTimeToActionable(base::TimeTicks::Now());
  }
  MaybeRecordOptInImpression();
}

void GlicInstanceMetrics::OnBind() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Bind"));
  bound_tab_count_++;
  if (bound_tab_count_ > max_concurrently_bound_tabs_) {
    max_concurrently_bound_tabs_ = bound_tab_count_;
  }
  LogEvent(GlicInstanceEvent::kTabBound);
}

void GlicInstanceMetrics::OnInstancePromoted() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Promoted"));
  LogEvent(GlicInstanceEvent::kInstancePromoted);
}

void GlicInstanceMetrics::OnWarmedInstanceCreated() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.CreatedWarmedInstance"));
  LogEvent(GlicInstanceEvent::kWarmedInstanceCreated);
}

void GlicInstanceMetrics::OnInstanceCreatedWithoutWarming() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.CreatedInstanceWithoutWarming"));
  LogEvent(GlicInstanceEvent::kInstanceCreatedWithoutWarming);
}

void GlicInstanceMetrics::OnSwitchFromConversation(
    const ShowOptions& show_options,
    std::optional<EmbedderKey> active_key) {
  if (std::holds_alternative<FloatingShowOptions>(
          show_options.embedder_options)) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.SwitchFromConversation.Floaty"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedFromFloaty);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Glic.Instance.SwitchFromConversation.SidePanel"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedFromSidePanel);
  }

  // If there's an active side panel, record the switch action on its helper.
  if (active_key) {
    if (auto* sp_key = std::get_if<SidePanelEmbedderKey>(&*active_key)) {
      tabs::TabInterface& tab = sp_key->tab.get();
      if (auto* helper = GlicInstanceHelper::From(&tab)) {
        helper->OnDaisyChainAction(
            DaisyChainFirstAction::kSwitchedConversation);
      }
    }
  }
}

void GlicInstanceMetrics::OnSwitchToConversation(
    const ShowOptions& show_options) {
  if (std::holds_alternative<FloatingShowOptions>(
          show_options.embedder_options)) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.SwitchToConversation.Floaty"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedToFloaty);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Glic.Instance.SwitchToConversation.SidePanel"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedToSidePanel);
  }
}

void GlicInstanceMetrics::OnShowInSidePanel(tabs::TabInterface* tab) {
  current_ui_mode_ = EmbedderType::kSidePanel;
  if (!tab) {
    return;
  }
  // Invocation source should always already be set by the time this is
  // called. Left as defensive measure.
  MaybeSetInitialInvocationSource(last_invocation_source_);

  if (side_panel_open_times_.contains(tab->GetHandle())) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kSidePanelOpenedWhileAlreadyOpen);
    return;
  }
  side_panel_open_times_[tab->GetHandle()] = base::TimeTicks::Now();
  if (std::ranges::find(tabs_with_side_panel_, tab->GetHandle()) ==
      tabs_with_side_panel_.end()) {
    tabs_with_side_panel_.push_back(tab->GetHandle());
  }
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.SidePanel"));
  LogEvent(GlicInstanceEvent::kSidePanelShown);
  LogEvent(GlicInstanceEvent::kShown);

  if (auto* helper = GlicInstanceHelper::From(tab)) {
    if (last_invocation_source_ ==
        mojom::InvocationSource::kNavigationCapture) {
      helper->SetIsDaisyChained(DaisyChainSource::kWebHandoff);
    } else if (last_invocation_source_ ==
               mojom::InvocationSource::kAutoOpenedForPdf) {
      helper->SetIsDaisyChained(DaisyChainSource::kAutoOpenPdf);
    }
  }

  if (last_invocation_source_ == mojom::InvocationSource::kAutoOpenedForPdf &&
      auto_open_pdf_source_id_ == ukm::kInvalidSourceId) {
    if (tab->GetContents() && tab->GetContents()->GetPrimaryMainFrame()) {
      auto_open_pdf_source_id_ =
          tab->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    }
    auto_open_pdf_start_time_ = base::TimeTicks::Now();
  }
}

void GlicInstanceMetrics::MaybeSetInitialInvocationSource(
    mojom::InvocationSource source) {
  if (initial_invocation_source_.has_value()) {
    return;
  }
  initial_invocation_source_ = source;

  // Backfill events that happened before the initial invocation source was set.
  const std::string source_str = GetInvocationSourceString(source);
  const std::string event_counts_histogram =
      base::StrCat({"InvocationSource.", source_str, ".EventCounts"});
  const std::string had_event_histogram =
      base::StrCat({"InvocationSource.", source_str, ".HadEvent"});
  for (const auto& [event, count] : event_counts_) {
    if (count <= 0) {
      continue;
    }
    metric_reporter_.RecordEnumeration(had_event_histogram, event);
    for (int i = 0; i < count; ++i) {
      metric_reporter_.RecordEnumeration(event_counts_histogram, event);
    }
  }
}

void GlicInstanceMetrics::OnShowInFloaty(const ShowOptions& options) {
  if (!floaty_open_time_.is_null()) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kFloatyOpenedWhileAlreadyOpen);
    return;
  }
  current_ui_mode_ = EmbedderType::kFloaty;
  floaty_open_time_ = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.Floaty"));
  LogEvent(GlicInstanceEvent::kFloatyShown);

  if (const auto* floaty_options =
          std::get_if<FloatingShowOptions>(&options.embedder_options)) {
    if (floaty_options->initial_mode != mojom::WebClientMode::kUnknown) {
      metric_reporter_.RecordEnumeration("Instance.Floaty.InitialMode",
                                         floaty_options->initial_mode);
    }
  }
  LogEvent(GlicInstanceEvent::kShown);
}

void GlicInstanceMetrics::OnFloatyClosed() {
  if (floaty_open_time_.is_null()) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kFloatyClosedWithoutOpen);
    return;
  }
  base::TimeDelta duration = base::TimeTicks::Now() - floaty_open_time_;
  metric_reporter_.RecordCustomTimes("Instance.Floaty.OpenDuration", duration,
                                     base::Milliseconds(1), base::Hours(1), 50);
  if (!first_floaty_close_recorded_) {
    first_floaty_close_recorded_ = true;
    mojom::InvocationSource source = initial_invocation_source_.value_or(
        mojom::InvocationSource::kUnsupported);
    metric_reporter_.RecordCustomTimes(
        base::StrCat({"InvocationSource.", GetInvocationSourceString(source),
                      ".FloatyFirstOpenDuration"}),
        duration, base::Milliseconds(1), base::Hours(1), 50);
  }
  floaty_open_time_ = base::TimeTicks();
}

void GlicInstanceMetrics::OnSidePanelClosed(
    tabs::TabInterface* tab,
    GlicInstanceMetrics::CloseReason reason) {
  if (!tab) {
    return;
  }

  if (auto* helper = GlicInstanceHelper::From(tab)) {
    if (reason == GlicInstanceMetrics::CloseReason::kTabSwitched) {
      helper->OnDaisyChainAction(DaisyChainFirstAction::kTabSwitched);
    } else {
      helper->OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    }
  }

  tabs::TabHandle tab_handle = tab->GetHandle();
  auto it = side_panel_open_times_.find(tab_handle);
  if (it == side_panel_open_times_.end()) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kSidePanelClosedWithoutOpen);
    return;
  }

  base::TimeDelta duration = base::TimeTicks::Now() - it->second;
  metric_reporter_.RecordCustomTimes("Instance.SidePanel.OpenDuration",
                                     duration, base::Milliseconds(1),
                                     base::Hours(1), 50);

  MaybeRecordFirstSidePanelOpenMetrics(duration);

  if (reason != CloseReason::kTabSwitched) {
    std::erase(tabs_with_side_panel_, tab->GetHandle());
    if (tabs_with_side_panel_.empty() &&
        initial_invocation_source_ ==
            mojom::InvocationSource::kAutoOpenedForPdf) {
      RecordAndResetAutoOpenPdfMetric();
    }
  }
  side_panel_open_times_.erase(it);
}

void GlicInstanceMetrics::MaybeRecordFirstSidePanelOpenMetrics(
    base::TimeDelta duration) {
  if (first_side_panel_close_recorded_) {
    return;
  }
  first_side_panel_close_recorded_ = true;
  mojom::InvocationSource source = initial_invocation_source_.value_or(
      mojom::InvocationSource::kUnsupported);
  const std::string source_str = GetInvocationSourceString(source);

  metric_reporter_.RecordCustomTimes(
      base::StrCat(
          {"InvocationSource.", source_str, ".SidePanelFirstOpenDuration"}),
      duration, base::Milliseconds(1), base::Hours(1), 50);

  const char* variant =
      (side_panel_prompt_count_ >= 1) ? "WithPrompts" : "NoPrompts";
  metric_reporter_.RecordCustomTimes(
      base::StrCat({"InvocationSource.", source_str,
                    ".SidePanelFirstOpenDuration.", variant}),
      duration, base::Milliseconds(1), base::Hours(1), 50);

  metric_reporter_.RecordCounts100(
      base::StrCat(
          {"InvocationSource.", source_str, ".SidePanelFirstOpenPromptCount"}),
      side_panel_prompt_count_);
}

void GlicInstanceMetrics::OnDetach() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Detach"));
  LogEvent(GlicInstanceEvent::kDetachedToFloaty);
}

void GlicInstanceMetrics::OnUnbindEmbedder(EmbedderKey key) {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.UnBind"));
  LogEvent(GlicInstanceEvent::kUnbindEmbedder);
  if (auto* sp_key = std::get_if<SidePanelEmbedderKey>(&key)) {
    tabs::TabInterface& tab = sp_key->tab.get();
    if (auto* helper = GlicInstanceHelper::From(&tab)) {
      // Log NoAction if instance is unbound before any other actions occur.
      helper->OnDaisyChainAction(DaisyChainFirstAction::kNoAction);
    }
    tabs::TabHandle tab_handle = tab.GetHandle();
    auto it = side_panel_open_times_.find(tab_handle);
    if (it != side_panel_open_times_.end()) {
      base::TimeDelta duration = base::TimeTicks::Now() - it->second;
      metric_reporter_.RecordCustomTimes("Instance.SidePanel.OpenDuration",
                                         duration, base::Milliseconds(1),
                                         base::Hours(1), 50);
      MaybeRecordFirstSidePanelOpenMetrics(duration);
      side_panel_open_times_.erase(it);
    } else {
      metric_reporter_.RecordEnumeration(
          "Instance.Metrics.Error",
          GlicInstanceMetricsError::kTabUnbindWithoutOpen);
    }
    tab_depths_.erase(tab_handle);
    if (bound_tab_count_ > 0) {
      bound_tab_count_--;
    }
  } else {
    if (!floaty_open_time_.is_null()) {
      base::TimeDelta duration = base::TimeTicks::Now() - floaty_open_time_;
      metric_reporter_.RecordCustomTimes("Instance.Floaty.OpenDuration",
                                         duration, base::Milliseconds(1),
                                         base::Hours(1), 50);
      if (!first_floaty_close_recorded_) {
        first_floaty_close_recorded_ = true;
        mojom::InvocationSource source = initial_invocation_source_.value_or(
            mojom::InvocationSource::kUnsupported);
        metric_reporter_.RecordCustomTimes(
            base::StrCat({"InvocationSource.",
                          GetInvocationSourceString(source),
                          ".FloatyFirstOpenDuration"}),
            duration, base::Milliseconds(1), base::Hours(1), 50);
      }
      floaty_open_time_ = base::TimeTicks();
    }
  }
}

void GlicInstanceMetrics::OnDaisyChain(DaisyChainSource source,
                                       bool success,
                                       tabs::TabInterface* new_tab,
                                       tabs::TabInterface* source_tab) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"Glic.Instance.DaisyChain.",
                    GetDaisyChainSourceString(source), ".",
                    success ? "Success" : "Failure"})
          .c_str()));
  if (success) {
    LogEvent(GlicInstanceEvent::kTabBoundViaDaisyChain);
    if (new_tab) {
      // Track the depth of tabs opened via daisy-chaining (one tab opening
      // another).
      int depth = 1;
      // If the new tab was opened from an existing tab, try to find the parent
      // tab's depth and increment it.
      if (source == DaisyChainSource::kTabContents && source_tab) {
        auto it = tab_depths_.find(source_tab->GetHandle());
        if (it != tab_depths_.end()) {
          depth = it->second + 1;
        }
      }
      tab_depths_[new_tab->GetHandle()] = depth;
      if (source == DaisyChainSource::kTabContents) {
        metric_reporter_.RecordExactLinear("Tab.DaisyChainDepth", depth, 50);
      }

      if (auto* new_helper = GlicInstanceHelper::From(new_tab)) {
        new_helper->SetIsDaisyChained(source);
      }
    }
    // If the new tab is opened from a daisy chain source, propagate the state
    // to the new tab's helper and record the recursive action on the source
    // tab.
    if (auto* source_helper = GlicInstanceHelper::From(source_tab)) {
      source_helper->OnDaisyChainAction(
          DaisyChainFirstAction::kRecursiveDaisyChain);
    }
  } else {
    LogEvent(GlicInstanceEvent::kDaisyChainFailed);
  }
}

void GlicInstanceMetrics::OnRegisterConversation(
    const std::string& conversation_id) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.RegisterConversation"));
  LogEvent(GlicInstanceEvent::kRegisterConversation);
}

void GlicInstanceMetrics::MaybeRecordTimeToDismissWhileLoading() {
  // If the client has not yet signaled ready, record the duration the user
  // waited from invocation until they dismissed (hid or closed) the panel.
  // We gate on `!has_logged_dismiss_while_loading` to ensure we only record
  // once per dismissal (e.g., avoiding duplicate logging if OnInstanceHidden
  // is followed by OnClose).
  if (invocation_load_state_.start_time.is_null() || is_client_ready_ ||
      invocation_load_state_.has_logged_dismiss_while_loading) {
    return;
  }
  base::TimeDelta wait_time =
      base::TimeTicks::Now() - invocation_load_state_.start_time;
  if (invocation_load_state_.embedder_type != EmbedderType::kUnknown) {
    metric_reporter_.RecordCustomTimes(
        base::StrCat(
            {"Instance.TimeToDismissWhileLoading.",
             GetEmbedderTypeString(invocation_load_state_.embedder_type)}),
        wait_time, base::Milliseconds(1), base::Seconds(60), 50);
  }
  invocation_load_state_.has_logged_dismiss_while_loading = true;
}

void GlicInstanceMetrics::OnInstanceHidden() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Hide"));
  LogEvent(GlicInstanceEvent::kInstanceHidden);
  MaybeRecordTimeToDismissWhileLoading();
  opt_in_impression_recorded_for_current_open_ = false;
  open_start_time_ = base::TimeTicks();
}

void GlicInstanceMetrics::OnClose() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Close"));
  LogEvent(GlicInstanceEvent::kClose);
  metric_reporter_.RecordEnumeration("PanelWebUiState.FinishState3",
                                     last_web_ui_state_);
  if (!GlicEnabling::HasConsentedForProfile(profile_)) {
    // Not split: consumer FRE/opt-in only; GEiC FRE tracked in b/571507463.
    base::UmaHistogramEnumeration("Glic.Fre.PanelWebUiState.FinishState",
                                  last_web_ui_state_);
  }
  MaybeRecordTimeToDismissWhileLoading();
  invocation_load_state_ = {};
  actionable_invocation_state_ = {};
  // A frame submitted during this invocation is stale for the next one.
  guest_actionable_.frame_submitted_while_hidden = false;
  opt_in_impression_recorded_for_current_open_ = false;
  open_start_time_ = base::TimeTicks();
}

bool GlicInstanceMetrics::MarkShownAndCheckIfFirstTime(EmbedderKey key) {
  // Returns true if the key was NOT in the set, and inserts it.
  // Returns false if the key WAS already in the set.
  return seen_embedders_.insert(ToSafeKey(key)).second;
}

bool GlicInstanceMetrics::ResetShownState(EmbedderKey key) {
  return seen_embedders_.erase(ToSafeKey(key)) > 0;
}

void GlicInstanceMetrics::OnOpen(glic::mojom::InvocationSource source,
                                 const ShowOptions& options) {
  const base::TimeTicks now = base::TimeTicks::Now();
  const EmbedderType embedder_type = GetEmbedderTypeFromShowOptions(options);
  open_start_time_ = now;
  was_client_ready_on_open_ = is_client_ready_;
  input_focused_for_current_open_ = false;
  if (!is_client_ready_) {
    if (invocation_load_state_.start_time.is_null()) {
      invocation_load_state_.start_time = now;
      invocation_load_state_.has_logged_dismiss_while_loading = false;
    }
    invocation_load_state_.embedder_type = embedder_type;
  }
  if (IsWaitingForActionable()) {
    if (actionable_invocation_state_.start_time.is_null()) {
      actionable_invocation_state_.start_time = now;
    }
    actionable_invocation_state_.embedder_type = embedder_type;
  }
  last_invocation_source_ = source;

  // 1. Log Initial Invocation Source
  if (!did_open_) {
    did_open_ = true;
    // Don't overwrite initial invocation source if it was set due to being
    // invoked in the background.
    MaybeSetInitialInvocationSource(source);
    metric_reporter_.RecordEnumeration("Instance.InitialInvocationSource",
                                       *initial_invocation_source_);
    if (!GlicEnabling::HasConsentedForProfile(profile_)) {
      // Not split: consumer FRE/opt-in only; GEiC FRE tracked in b/571507463.
      base::UmaHistogramEnumeration("Glic.Fre.Invoked.InvocationSource",
                                    *initial_invocation_source_);
      base::RecordAction(base::UserMetricsAction("Glic.Fre.Invoked"));
    }
  }

  // 2. Log Events
  LogEvent(GlicInstanceEvent::kOpen);
  if (!GlicEnabling::HasConsentedForProfile(profile_)) {
    auto fre_tracker = std::make_unique<GlicFreImpressionCuiTracker>();
    if (opt_in_impression_recorded_for_current_open_) {
      fre_tracker->OnEvent(GlicInstanceEvent::kOptInImpression);
      opt_in_impression_recorded_for_current_open_ = false;
    } else {
      cui_trackers_.push_back(std::move(fre_tracker));
    }
  }

  // 3. Record Actions
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Open"));

  // 4. Log Open Source
  bool is_floaty =
      std::holds_alternative<FloatingShowOptions>(options.embedder_options);
  std::string open_source_base = is_floaty ? "Instance.Floaty.OpenSource"
                                           : "Instance.SidePanel.OpenSource";
  metric_reporter_.RecordEnumeration(open_source_base, source);

  // 5. Log Zoom Level
  if (pref_service_) {
    int zoom_level = pref_service_->GetInteger(prefs::kGlicZoomLevel);
    metric_reporter_.RecordSparse("ZoomLevel.OnOpen", zoom_level);
  }

  // 6. SaaS Usage
  if (saas_usage_reporting_controller_ && !saas_usage_recorded_) {
    saas_usage_reporting_controller_->RecordGeminiInChromeUsage();
    saas_usage_recorded_ = true;
  }
}

void GlicInstanceMetrics::OnToggle(glic::mojom::InvocationSource source,
                                   const EmbedderKey& embedder_key,
                                   bool is_showing) {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Toggle"));
  if (std::holds_alternative<FloatingEmbedderKey>(embedder_key)) {
    metric_reporter_.RecordEnumeration("Instance.Floaty.ToggleSource", source);
  } else {
    metric_reporter_.RecordEnumeration("Instance.SidePanel.ToggleSource",
                                       source);
  }
  LogEvent(GlicInstanceEvent::kToggle);
}

void GlicInstanceMetrics::OnBoundTabDestroyed() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.BoundTabDestroyed"));
  LogEvent(GlicInstanceEvent::kBoundTabDestroyed);
}

void GlicInstanceMetrics::OnCreateTab() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.CreateTab"));
  LogEvent(GlicInstanceEvent::kCreateTab);
}

void GlicInstanceMetrics::OnCreateTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.CreateTask"));
  LogEvent(GlicInstanceEvent::kCreateTask);
}

void GlicInstanceMetrics::OnPerformActions() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.PerformActions"));
  LogEvent(GlicInstanceEvent::kPerformActions);
}

void GlicInstanceMetrics::OnStopActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.StopActorTask"));
  LogEvent(GlicInstanceEvent::kStopActorTask);
}

void GlicInstanceMetrics::OnPauseActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.PauseActorTask"));
  LogEvent(GlicInstanceEvent::kPauseActorTask);
}

void GlicInstanceMetrics::OnResumeActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.ResumeActorTask"));
  LogEvent(GlicInstanceEvent::kResumeActorTask);
}

void GlicInstanceMetrics::InterruptActorTask() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.InterruptActorTask"));
  LogEvent(GlicInstanceEvent::kInterruptActorTask);
}

void GlicInstanceMetrics::UninterruptActorTask() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.UninterruptActorTask"));
  LogEvent(GlicInstanceEvent::kUninterruptActorTask);
}

void GlicInstanceMetrics::SetActiveInvocationId(
    std::optional<uint64_t> invocation_id) {
  active_invocation_id_ = invocation_id;
  active_invocation_start_time_ =
      invocation_id ? base::TimeTicks::Now() : base::TimeTicks();
  input_focused_for_current_invocation_ = false;
}

void GlicInstanceMetrics::OnClientLoadError(ClientLoadErrorReason reason) {
#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  metrics::structured::events::v2::glic::ClientLoadError event;
  event.SetReason(ToStructuredReason(reason));
  if (active_invocation_id_.has_value()) {
    event.SetInvocationId(*active_invocation_id_);
    event.SetElapsedMs((base::TimeTicks::Now() - active_invocation_start_time_)
                           .InMilliseconds());
  }
  metrics::structured::StructuredMetricsClient::Record(std::move(event));
#endif
}

void GlicInstanceMetrics::OnWebUiStateChanged(mojom::WebUiState state) {
#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  if (active_invocation_id_.has_value()) {
    metrics::structured::StructuredMetricsClient::Record(
        metrics::structured::events::v2::glic::InvokeWebUiStateReached()
            .SetInvocationId(*active_invocation_id_)
            .SetWebUiState(static_cast<int>(state)));
  }
#endif

  last_web_ui_state_ = state;
  switch (state) {
    case mojom::WebUiState::kUninitialized:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Uninitialized"));
      LogEvent(GlicInstanceEvent::kWebUiStateUninitialized);
      break;
    case mojom::WebUiState::kBeginLoad:
      web_ui_load_start_time_ = base::TimeTicks::Now();
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.BeginLoad"));
      LogEvent(GlicInstanceEvent::kWebUiStateBeginLoad);
      break;
    case mojom::WebUiState::kShowLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.ShowLoading"));
      LogEvent(GlicInstanceEvent::kWebUiStateShowLoading);
      break;
    case mojom::WebUiState::kHoldLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.HoldLoading"));
      LogEvent(GlicInstanceEvent::kWebUiStateHoldLoading);
      break;
    case mojom::WebUiState::kFinishLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.FinishLoading"));
      LogEvent(GlicInstanceEvent::kWebUiStateFinishLoading);
      break;
    case mojom::WebUiState::kError:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Error"));
      LogEvent(GlicInstanceEvent::kWebUiStateError);
      break;
    case mojom::WebUiState::kOffline:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Offline"));
      LogEvent(GlicInstanceEvent::kWebUiStateOffline);
      break;
    case mojom::WebUiState::kUnavailable:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Unavailable"));
      LogEvent(GlicInstanceEvent::kWebUiStateUnavailable);
      break;
    case mojom::WebUiState::kIneligibleAccount:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.IneligibleAccount"));
      LogEvent(GlicInstanceEvent::kWebUiStateIneligibleAccount);
      break;
    case mojom::WebUiState::kReady: {
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Ready"));
      LogEvent(GlicInstanceEvent::kWebUiStateReady);
      if (!web_ui_load_start_time_.is_null()) {
        base::TimeDelta load_time =
            base::TimeTicks::Now() - web_ui_load_start_time_;
        std::string_view visibility_suffix =
            (visibility_tracker_ && visibility_tracker_->state())
                ? ".Visible"
                : ".Nonvisible";
        metric_reporter_.RecordCustomTimes(
            base::StrCat({"Instance.WebUiLoadTime", visibility_suffix}),
            load_time, base::Milliseconds(1), base::Seconds(60), 50);

        if (profile_ && !has_consented_) {
          has_consented_ = GlicEnabling::HasConsentedForProfile(profile_);
          if (!has_consented_) {
            // Not split: consumer FRE/opt-in only; GEiC FRE tracked in
            // b/571507463.
            base::UmaHistogramCustomTimes(
                base::StrCat(
                    {"Glic.Onboarding.WebUiLoadTime", visibility_suffix}),
                load_time, base::Milliseconds(1), base::Seconds(60), 50);
          }
        }

        if (initial_invocation_source_.has_value()) {
          metric_reporter_.RecordCustomTimes(
              base::StrCat({"InvocationSource.",
                            GetInvocationSourceString(
                                initial_invocation_source_.value()),
                            ".WebUiLoadTime", visibility_suffix}),
              load_time, base::Milliseconds(1), base::Seconds(60), 50);
        }
        web_ui_load_start_time_ = base::TimeTicks();
      }
      break;
    }
    case mojom::WebUiState::kUnresponsive:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Unresponsive"));
      LogEvent(GlicInstanceEvent::kWebUiStateUnresponsive);
      break;
    case mojom::WebUiState::kSignIn:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.SignIn"));
      LogEvent(GlicInstanceEvent::kWebUiStateSignIn);
      break;
    case mojom::WebUiState::kGuestError:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.GuestError"));
      LogEvent(GlicInstanceEvent::kWebUiStateGuestError);
      break;
    case mojom::WebUiState::kDisabledByAdmin:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.DisabledByAdmin"));
      LogEvent(GlicInstanceEvent::kWebUiStateDisabledByAdmin);
      break;
    case mojom::WebUiState::kWarmed:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Warmed"));
      LogEvent(GlicInstanceEvent::kWebUiStateWarmed);
      break;
    case mojom::WebUiState::kLocationMismatch:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.LocationMismatch"));
      LogEvent(GlicInstanceEvent::kWebUiStateLocationMismatch);
      break;
    case mojom::WebUiState::kGuestLoaded:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.GuestLoaded"));
      LogEvent(GlicInstanceEvent::kWebUiStateGuestLoaded);
      break;
  }
}

void GlicInstanceMetrics::OnClientReady() {
  is_client_ready_ = true;
  client_ready_time_ = base::TimeTicks::Now();
  input_focused_after_client_ready_ = false;
  MaybeRecordOptInImpression();
  LogEvent(GlicInstanceEvent::kClientReady);

  if (invocation_load_state_.start_time.is_null()) {
    return;
  }
  base::TimeDelta presentation_time =
      base::TimeTicks::Now() - invocation_load_state_.start_time;
  if (invocation_load_state_.embedder_type != EmbedderType::kUnknown) {
    bool is_visible =
        visibility_tracker_ ? visibility_tracker_->state() : false;
    std::string_view inactive_str = is_visible ? "" : "Inactive.";
    std::string histogram_name = base::StrCat(
        {"Instance.PanelPresentationTime.", inactive_str,
         GetEmbedderTypeString(invocation_load_state_.embedder_type)});
    metric_reporter_.RecordCustomTimes(histogram_name, presentation_time,
                                       base::Milliseconds(1), base::Seconds(60),
                                       50);
  }
  invocation_load_state_ = {};
}

void GlicInstanceMetrics::OnGuestActionableMarkObserved(
    base::TimeTicks mark_time) {
  if (guest_actionable_.actionable_mark_time.has_value()) {
    return;
  }
  guest_actionable_.actionable_mark_time = mark_time;
  const bool is_visible =
      visibility_tracker_ ? visibility_tracker_->state() : false;
  // When the panel is visible, active TimeToActionable waits for
  // OnGuestFrameSubmitted() so it includes the compositor frame that renders
  // the actionable state. When the panel was hidden before the mark arrived
  // (but not yet closed), no visible frame will be produced, so record the
  // Inactive variant immediately using the mark's timestamp.
  if (!is_visible && !actionable_invocation_state_.start_time.is_null() &&
      mark_time >= actionable_invocation_state_.start_time) {
    RecordTimeToActionable(mark_time);
  }
}

void GlicInstanceMetrics::OnGuestFrameSubmitted() {
  const bool is_visible =
      visibility_tracker_ ? visibility_tracker_->state() : false;
  if (is_visible) {
    RecordTimeToActionable(base::TimeTicks::Now());
  } else if (!actionable_invocation_state_.start_time.is_null()) {
    // `InsertVisualStateCallback` is requested while the guest is mounted and
    // visible, but the renderer's compositor frame can arrive after the panel
    // was hidden (during the 500ms hide-to-close debounce, while the guest is
    // still mounted). Remember it so if the user shows the panel again before
    // OnClose(), we still record when it appears on screen.
    guest_actionable_.frame_submitted_while_hidden = true;
  }
}

void GlicInstanceMetrics::OnGuestReset() {
  guest_actionable_ = {};
}

bool GlicInstanceMetrics::IsWaitingForActionable() const {
  return !guest_actionable_.recorded;
}

void GlicInstanceMetrics::RecordTimeToActionable(base::TimeTicks end_time) {
  if (guest_actionable_.recorded) {
    return;
  }
  guest_actionable_.recorded = true;
  if (actionable_invocation_state_.start_time.is_null()) {
    return;
  }

  base::TimeDelta actionable_time =
      end_time - actionable_invocation_state_.start_time;
  if (actionable_invocation_state_.embedder_type != EmbedderType::kUnknown) {
    bool is_visible =
        visibility_tracker_ ? visibility_tracker_->state() : false;
    std::string_view inactive_str = is_visible ? "" : "Inactive.";
    std::string histogram_name = base::StrCat(
        {"Instance.TimeToActionable.", inactive_str,
         GetEmbedderTypeString(actionable_invocation_state_.embedder_type)});
    metric_reporter_.RecordCustomTimes(histogram_name, actionable_time,
                                       base::Milliseconds(1), base::Seconds(60),
                                       50);
  }
  actionable_invocation_state_ = {};
}

void GlicInstanceMetrics::LogEvent(GlicInstanceEvent event) {
  for (auto it = cui_trackers_.begin(); it != cui_trackers_.end();) {
    if ((*it)->OnEvent(event)) {
      it = cui_trackers_.erase(it);
    } else {
      ++it;
    }
  }
  metric_reporter_.RecordEnumeration("Instance.EventCounts", event);
  if (initial_invocation_source_.has_value()) {
    metric_reporter_.RecordEnumeration(
        base::StrCat(
            {"InvocationSource.",
             GetInvocationSourceString(initial_invocation_source_.value()),
             ".EventCounts"}),
        event);
  }
  if (event_counts_[event] == 0) {
    // This is recorded only the first time an event occurs within this sessions
    // lifetime.
    metric_reporter_.RecordEnumeration("Instance.HadEvent", event);
    if (initial_invocation_source_.has_value()) {
      metric_reporter_.RecordEnumeration(
          base::StrCat(
              {"InvocationSource.",
               GetInvocationSourceString(initial_invocation_source_.value()),
               ".HadEvent"}),
          event);
    }
  }
  event_counts_[event]++;

  session_manager_.OnEvent(event);
}

int GlicInstanceMetrics::GetEventCount(GlicInstanceEvent event) {
  const auto it = event_counts_.find(event);
  return it == event_counts_.end() ? 0 : it->second;
}

void GlicInstanceMetrics::OnUserInputSubmitted(mojom::WebClientMode mode,
                                               mojom::PromptType prompt_type) {
  if (current_ui_mode_ == EmbedderType::kSidePanel) {
    side_panel_prompt_count_++;

    // Try to attribute the input submission to the currently focused tab for
    // daisy chain metrics.
    if (sharing_manager_) {
      if (auto* tab = sharing_manager_->GetFocusedTabData().focus()) {
        if (auto* helper = GlicInstanceHelper::From(tab)) {
          helper->OnDaisyChainAction(DaisyChainFirstAction::kInputSubmitted);
        }
      }
    }
  }

  if (turn_.response_started_) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kInputSubmittedWhileResponseInProgress);
    return;
  }
  if (!visibility_tracker_->state()) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kInputSubmittedWhileHidden);
    return;
  }
  session_manager_.OnUserInputSubmitted(mode);
  LogEvent(GlicInstanceEvent::kUserInputSubmitted);
  cui_trackers_.push_back(std::make_unique<GlicSubmitQueryCuiTracker>());
  cui_trackers_.push_back(std::make_unique<GlicResponseCompletionCuiTracker>());

  base::RecordAction(base::UserMetricsAction("GlicResponseInputSubmit"));
  metric_reporter_.RecordEnumeration("Turn.PromptType", prompt_type);

  if (sharing_manager_) {
    RecordSelectionOverlayMetrics(sharing_manager_->GetPinnedTabs(),
                                  metric_reporter_);
  }

  // Reset turn data and start populating it for the new turn being started.
  turn_ = {};
  turn_.input_submitted_time_ = base::TimeTicks::Now();
  if (sharing_manager_) {
    // Use the focused tab for UKM source if available. If no tab is focused,
    // leave turn_.chosen_source_id_ as its default of NoURLSourceId.
    turn_.chosen_source_id_ =
        GetUkmSourceIdForTab(sharing_manager_->GetFocusedTabData().focus());
  }

  turn_.ui_mode_ = current_ui_mode_;
  turn_.input_mode_ = mode;
  input_mode_ = mode;
  inputs_modes_used_.Put(mode);
}

void GlicInstanceMetrics::DidRequestContextFromFocusedTab() {
  LogEvent(GlicInstanceEvent::kFocusedTabContextRequested);
  DidRequestContextFromTab();
}

void GlicInstanceMetrics::DidRequestContextFromTab() {
  LogEvent(GlicInstanceEvent::kTabContextRequested);
  turn_.did_request_context_ = true;
}

void GlicInstanceMetrics::OnResponseStarted() {
  LogEvent(GlicInstanceEvent::kResponseStarted);
  base::RecordAction(base::UserMetricsAction("GlicResponseStart"));
  turn_.response_started_ = true;

  bool is_response_for_actuation =
      !turn_.action_result_submitted_time_.is_null();
  if (is_response_for_actuation) {
    metric_reporter_.RecordMediumTimes(
        "Turn.Actuation.ResponseStartTime",
        base::TimeTicks::Now() - turn_.action_result_submitted_time_);
  }

  // It doesn't make sense to record response start without input submission,
  // unless we just submitted an actuation result.
  if (turn_.input_submitted_time_.is_null() && !is_response_for_actuation) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kResponseStartWithoutInput);
    return;
  }

  if (!visibility_tracker_->state()) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kResponseStartWhileHidingOrHidden);
    return;
  }

  base::RecordAction(base::UserMetricsAction("GlicResponse"));

  if (!turn_.input_submitted_time_.is_null()) {
    base::TimeDelta start_time =
        base::TimeTicks::Now() - turn_.input_submitted_time_;
    metric_reporter_.RecordMediumTimes("Turn.ResponseStartTime", start_time);
    std::string_view mode_string = GetInputModeString(input_mode_);
    metric_reporter_.RecordMediumTimes(
        base::StrCat({"Turn.ResponseStartTime.InputMode.", mode_string}),
        start_time);

    if (turn_.did_request_context_) {
      metric_reporter_.RecordMediumTimes("Turn.ResponseStartTime.WithContext",
                                         start_time);
    } else {
      metric_reporter_.RecordMediumTimes(
          "Turn.ResponseStartTime.WithoutContext", start_time);
    }
  }

  metric_reporter_.RecordEnumeration(
      "Response.Segmentation",
      GetResponseSegmentation(turn_.ui_mode_ == EmbedderType::kSidePanel,
                              turn_.input_mode_, last_invocation_source_));

  ukm::builders::Glic_Response(turn_.chosen_source_id_)
      .SetAttached(turn_.ui_mode_ == EmbedderType::kSidePanel)
      .SetInvocationSource(static_cast<int64_t>(last_invocation_source_))
      .SetWebClientMode(static_cast<int64_t>(turn_.input_mode_))
      .Record(ukm::UkmRecorder::Get());
}

void GlicInstanceMetrics::OnResponseStopped(mojom::ResponseStopCause cause) {
  bool has_input = !turn_.input_submitted_time_.is_null();
  bool has_action_result = !turn_.action_result_submitted_time_.is_null();

  // The client may call "stopped" without "started" for very short responses.
  // We synthetically call it ourselves in this case.
  if ((has_input || has_action_result) && !turn_.response_started_) {
    OnResponseStarted();
  }

  base::RecordAction(base::UserMetricsAction("GlicResponseStop"));
  std::string_view cause_suffix;
  switch (cause) {
    case mojom::ResponseStopCause::kUser:
      cause_suffix = ".ByUser";
      base::RecordAction(base::UserMetricsAction("GlicResponseStopByUser"));
      LogEvent(GlicInstanceEvent::kResponseStoppedByUser);
      break;
    case mojom::ResponseStopCause::kOther:
      cause_suffix = ".Other";
      base::RecordAction(base::UserMetricsAction("GlicResponseStopOther"));
      LogEvent(GlicInstanceEvent::kResponseStoppedOther);
      break;
    case mojom::ResponseStopCause::kUnknown:
      // `kUnknown` represents an omitted stop cause (`cause === undefined`)
      // from the web client, indicating normal, successful completion.
      cause_suffix = ".UnknownCause";
      base::RecordAction(
          base::UserMetricsAction("GlicResponseStopUnknownCause"));
      LogEvent(GlicInstanceEvent::kResponseStoppedSuccess);
      break;
  }

  if (!has_input && !has_action_result) {
    metric_reporter_.RecordEnumeration(
        "Instance.Metrics.Error",
        GlicInstanceMetricsError::kResponseStopWithoutInput);
    metric_reporter_.RecordEnumeration(
        base::StrCat({"Instance.Metrics.Error", cause_suffix}),
        GlicInstanceMetricsError::kResponseStopWithoutInput);
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta latency;
    if (has_input) {
      latency = now - turn_.input_submitted_time_;
      metric_reporter_.RecordMediumTimes("Turn.ResponseStopTime", latency);
      metric_reporter_.RecordMediumTimes(
          base::StrCat({"Turn.ResponseStopTime", cause_suffix}), latency);
    } else if (has_action_result) {
      latency = now - turn_.action_result_submitted_time_;
      metric_reporter_.RecordMediumTimes("Turn.Actuation.ResponseStopTime",
                                         latency);
      metric_reporter_.RecordMediumTimes(
          base::StrCat({"Turn.Actuation.ResponseStopTime", cause_suffix}),
          latency);
    }
    RecordResponseLatencyByAttachedTabCount(latency);
  }

  GlicTurnSource turn_source;
  switch (turn_.ui_mode_) {
    case EmbedderType::kSidePanel:
      switch (turn_.input_mode_) {
        case mojom::WebClientMode::kText:
          turn_source = GlicTurnSource::kSidePanelText;
          break;
        case mojom::WebClientMode::kAudio:
          turn_source = GlicTurnSource::kSidePanelAudio;
          break;
        case mojom::WebClientMode::kUnknown:
          turn_source = GlicTurnSource::kUnknown;
          break;
      }
      break;
    case EmbedderType::kFloaty:
      switch (turn_.input_mode_) {
        case mojom::WebClientMode::kText:
          turn_source = GlicTurnSource::kFloatyText;
          break;
        case mojom::WebClientMode::kAudio:
          turn_source = GlicTurnSource::kFloatyAudio;
          break;
        case mojom::WebClientMode::kUnknown:
          turn_source = GlicTurnSource::kUnknown;
          break;
      }
      break;
    case EmbedderType::kTab:
      switch (turn_.input_mode_) {
        case mojom::WebClientMode::kText:
          turn_source = GlicTurnSource::kTabText;
          break;
        case mojom::WebClientMode::kAudio:
          turn_source = GlicTurnSource::kTabAudio;
          break;
        case mojom::WebClientMode::kUnknown:
          turn_source = GlicTurnSource::kUnknown;
          break;
      }
      break;
    case EmbedderType::kUnknown:
      turn_source = GlicTurnSource::kUnknown;
      break;
  }
  metric_reporter_.RecordEnumeration("Turn.Source", turn_source);
  metric_reporter_.RecordEnumeration("Turn.InvocationSource",
                                     last_invocation_source_);
  // Reset the turn.
  last_turn_ = turn_;
  turn_ = {};
}

void GlicInstanceMetrics::OnTurnCompleted(mojom::WebClientModel model,
                                          base::TimeDelta duration) {
  LogEvent(GlicInstanceEvent::kTurnCompleted);
  metric_reporter_.RecordMediumTimes(model == mojom::WebClientModel::kActor
                                         ? "Turn.Duration.Actor"
                                         : "Turn.Duration.Default",
                                     duration);
}

void GlicInstanceMetrics::OnUserResizeStarted(const gfx::Size& start_size) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.Floaty.UserResizeStarted"));
  metric_reporter_.RecordCounts10000("Instance.Floaty.UserResizeStarted.Width",
                                     start_size.width());
  metric_reporter_.RecordCounts10000("Instance.Floaty.UserResizeStarted.Height",
                                     start_size.height());
}

void GlicInstanceMetrics::OnUserResizeEnded(const gfx::Size& end_size) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.Floaty.UserResizeEnded"));
  metric_reporter_.RecordCounts10000("Instance.Floaty.UserResizeEnded.Width",
                                     end_size.width());
  metric_reporter_.RecordCounts10000("Instance.Floaty.UserResizeEnded.Height",
                                     end_size.height());
}

void GlicInstanceMetrics::OnReaction(
    mojom::MetricUserInputReactionType reaction_type) {
  LogEvent(GlicInstanceEvent::kReaction);

  switch (reaction_type) {
    case mojom::MetricUserInputReactionType::kUnknown:
      base::RecordAction(base::UserMetricsAction("GlicReactionUnknown"));
      return;
    case mojom::MetricUserInputReactionType::kCanned:
      base::RecordAction(base::UserMetricsAction("GlicReactionCanned"));
      return;
    case mojom::MetricUserInputReactionType::kModel:
      base::RecordAction(base::UserMetricsAction("GlicReactionModelled"));
      return;
  }
}

void GlicInstanceMetrics::OnInputFocused() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.InputFocused"));
  LogEvent(GlicInstanceEvent::kInputFocused);
  const base::TimeTicks now = base::TimeTicks::Now();
  const bool is_non_onboarded = !GlicEnabling::HasConsentedForProfile(profile_);
  auto record_latency = [&](std::string_view name, base::TimeDelta duration) {
    metric_reporter_.RecordMediumTimes(name, duration);
    if (is_non_onboarded) {
      metric_reporter_.RecordMediumTimes(base::StrCat({name, ".NonOnboarded"}),
                                         duration);
    }
  };

  if (!open_start_time_.is_null() && !input_focused_for_current_open_) {
    input_focused_for_current_open_ = true;
    input_focused_for_current_invocation_ = true;
    const base::TimeDelta duration = now - open_start_time_;
    record_latency("Instance.TimeToInputFocus", duration);
    record_latency(was_client_ready_on_open_ ? "Instance.TimeToInputFocus.Warm"
                                             : "Instance.TimeToInputFocus.Cold",
                   duration);
  } else if (!active_invocation_start_time_.is_null() &&
             !input_focused_for_current_invocation_) {
    input_focused_for_current_invocation_ = true;
    const base::TimeDelta duration = now - active_invocation_start_time_;
    record_latency("Instance.TimeToInputFocus", duration);
    record_latency(is_client_ready_ ? "Instance.TimeToInputFocus.Warm"
                                    : "Instance.TimeToInputFocus.Cold",
                   duration);
  }
  if (!client_ready_time_.is_null() && !input_focused_after_client_ready_) {
    input_focused_after_client_ready_ = true;
    record_latency("Instance.ClientReadyToInputFocus",
                   now - client_ready_time_);
  }
}

void GlicInstanceMetrics::OnActionSubmitted(bool is_retry) {
  turn_.action_result_submitted_time_ = base::TimeTicks::Now();
  metric_reporter_.RecordBoolean("Turn.Actuation.IsRetry", is_retry);
}

void GlicInstanceMetrics::OnSessionStarted() {
  session_count_++;

  // If `last_session_end_time_` is not null, we can record the time between
  // sessions.
  if (!last_session_end_time_.is_null()) {
    const base::TimeDelta time_between_sessions =
        base::TimeTicks::Now() - last_session_end_time_;
    metric_reporter_.RecordCustomTimes("Instance.TimeBetweenSessions.7D",
                                       time_between_sessions, base::Seconds(1),
                                       base::Days(7), 50);
    metric_reporter_.RecordCustomTimes("Instance.TimeBetweenSessions.24H",
                                       time_between_sessions, base::Seconds(1),
                                       base::Hours(24), 50);
  }
}

void GlicInstanceMetrics::OnSessionFinished() {
  if (base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
    metric_reporter_.RecordCounts100("ScrollTo.SessionCount",
                                     scroll_attempt_count_);
    scroll_attempt_count_ = 0;
  }
  last_session_end_time_ = base::TimeTicks::Now();
}

void GlicInstanceMetrics::OnZoomLevelChange() {
  ++zoom_change_count_;
}

void GlicInstanceMetrics::RecordAttachedContextTabCount(int tab_count) {
  metric_reporter_.RecordExactLinear("Response.AttachedContextCount", tab_count,
                                     51);
}

void GlicInstanceMetrics::RecordResponseLatencyByAttachedTabCount(
    base::TimeDelta latency) {
  std::string tab_count_suffix;
  if (pinned_tab_count_ > 10) {
    tab_count_suffix = "MoreThan10";
  } else {
    tab_count_suffix = base::NumberToString(pinned_tab_count_);
  }

  metric_reporter_.RecordMediumTimes(
      base::StrCat({"Turn.LatencyByAttachedTabCount.", tab_count_suffix}),
      latency);
}

int GlicInstanceMetrics::GetPinnedTabCount() const {
  return pinned_tab_count_;
}

void GlicInstanceMetrics::RecordSkillsWebClientEvent(
    mojom::SkillsWebClientEvent event) {
  using enum mojom::SkillsWebClientEvent;

  // Exclude the internal system and progression steps of the Skill Builder.
  // Note: kSkillBuilderClickedPromoChip is intentionally not excluded here
  // because it represents a direct user engagement click in the main UI,
  // acting as the entry point into the funnel.
  if (event != kSkillBuilderPromptGenerated &&
      event != kSkillBuilderClickedSaveAsSkill) {
    metric_reporter_.RecordEnumeration("Skills.WebClient.Event", event);
  }

  switch (event) {
    case kOpenedMenu:
      metric_reporter_.RecordEnumeration("Skills.Invoke.Funnel",
                                         SkillsInvokeFunnel::kOpenedMenu);
      break;
    // --- Skill Invocation Metrics ---
    case kUsedFirstPartySkill:
      skills::RecordSkillsInvokeAction(skills::SkillsInvokeAction::kFirstParty);
      metric_reporter_.RecordEnumeration("Skills.Invoke.Funnel",
                                         SkillsInvokeFunnel::kInvokedSkill);
      break;
    case kUsedUserCreatedSkill:
      skills::RecordSkillsInvokeAction(
          skills::SkillsInvokeAction::kUserCreated);
      metric_reporter_.RecordEnumeration("Skills.Invoke.Funnel",
                                         SkillsInvokeFunnel::kInvokedSkill);
      break;
    case kUsedDerivedFirstPartySkill:
      skills::RecordSkillsInvokeAction(
          skills::SkillsInvokeAction::kDerivedFromFirstParty);
      metric_reporter_.RecordEnumeration("Skills.Invoke.Funnel",
                                         SkillsInvokeFunnel::kInvokedSkill);
      break;
    case kClickedManageFromMenu:
    case kClickedAddFromMenu:
    case kClickedSaveAsSkillHoverChip:
    case kClickedEditSkillHoverChip:
    case kClickedAddOn1pSkill:
    case kClickedEditFromMenu:
    case kClickedBrowseSkillFromPlusMenu:
    case kClickedSkillFromPlusMenu:
    case kClickedManageSkillFromPlusMenu:
    case kClickedMoreFromMenu:
    case kClickedManageSkillFromMenu:
    case kClickedSkillFromMenu:
    case kClickedBrowseSkillsFromMenu:
      break;
    // --- SkillBuilder Metrics ---
    case kSkillBuilderClickedPromoChip:
      metric_reporter_.RecordEnumeration("Skills.SkillBuilder.Event",
                                         SkillBuilderEvent::kClickedPromoChip);
      break;
    case kSkillBuilderPromptGenerated:
      metric_reporter_.RecordEnumeration("Skills.SkillBuilder.Event",
                                         SkillBuilderEvent::kPromptGenerated);
      break;
    case kSkillBuilderClickedSaveAsSkill:
      metric_reporter_.RecordEnumeration(
          "Skills.SkillBuilder.Event", SkillBuilderEvent::kClickedSaveAsSkill);
      break;
    // --- No-Ops ---
    case mojom::SkillsWebClientEvent::kUnknown:
      break;
  }
}

void GlicInstanceMetrics::RecordAndResetAutoOpenPdfMetric() {
  if (auto_open_pdf_source_id_ == ukm::kInvalidSourceId) {
    return;
  }
  base::TimeDelta auto_open_duration;
  if (!auto_open_pdf_start_time_.is_null()) {
    auto_open_duration = base::TimeTicks::Now() - auto_open_pdf_start_time_;
  }
  int64_t bucketed_duration_ms = ukm::GetExponentialBucketMinForUserTiming(
      auto_open_duration.InMilliseconds());
  ukm::builders::Glic_AutoOpen_Closed(auto_open_pdf_source_id_)
      .SetSessionDurationMs(bucketed_duration_ms)
      .Record(ukm::UkmRecorder::Get());
  auto_open_pdf_source_id_ = ukm::kInvalidSourceId;
  auto_open_pdf_start_time_ = base::TimeTicks();
}

}  // namespace glic
