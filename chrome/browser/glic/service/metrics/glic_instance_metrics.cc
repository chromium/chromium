// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"

#include <cmath>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_state_tracker.h"
#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

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

std::string GetDaisyChainSourceString(DaisyChainSource source) {
  switch (source) {
    case DaisyChainSource::kGlicContents:
      return "GlicContents";
    case DaisyChainSource::kTabContents:
      return "TabContents";
    case DaisyChainSource::kActorAddTab:
      return "ActorAddTab";
    default:
      return "Unknown";
  }
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
  kMaxValue = kFloatyAudio,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicTurnSource)

}  // namespace

GlicInstanceMetrics::GlicInstanceMetrics() : session_manager_(this) {
  // Used in the unit tests.
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Created"));
  activity_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedActiveDuration");
  visibility_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedVisibleDuration");
  LogEvent(GlicInstanceEvent::kInstanceCreated);
}

GlicInstanceMetrics::GlicInstanceMetrics(GlicSharingManager* sharing_manager)
    : session_manager_(this),
      pinned_tabs_changed_subscription_(
          sharing_manager->AddPinnedTabsChangedCallback(
              base::BindRepeating(&GlicInstanceMetrics::OnPinnedTabsChanged,
                                  base::Unretained(this)))),
      sharing_manager_(sharing_manager) {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Created"));
  activity_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedActiveDuration");
  visibility_tracker_ = std::make_unique<GlicStateTracker>(
      false, "Glic.Instance.UninterruptedVisibleDuration");
  LogEvent(GlicInstanceEvent::kInstanceCreated);
}

GlicInstanceMetrics::~GlicInstanceMetrics() {
  OnInstanceDestroyed();
}

void GlicInstanceMetrics::OnPinnedTabsChanged(
    const std::vector<content::WebContents*>& pinned_contents) {
  pinned_tab_count_ = pinned_contents.size();
  session_manager_.SetPinnedTabCount(pinned_tab_count_);
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

  base::UmaHistogramCustomTimes("Glic.Instance.TotalActiveDuration",
                                total_active_time, base::Milliseconds(1),
                                base::Hours(24), 50);
  base::UmaHistogramCustomTimes("Glic.Instance.TotalBackgroundDuration",
                                background_time, base::Milliseconds(1),
                                base::Hours(24), 50);
  base::UmaHistogramCustomTimes("Glic.Instance.TotalVisibleDuration",
                                total_visible_time, base::Milliseconds(1),
                                base::Hours(24), 50);
  base::UmaHistogramCustomTimes("Glic.Instance.TotalHiddenDuration",
                                hidden_time, base::Milliseconds(1),
                                base::Hours(24), 50);

  base::UmaHistogramCustomTimes("Glic.Instance.LifetimeDuration", lifetime,
                                base::Milliseconds(1), base::Hours(24), 50);
  base::UmaHistogramCustomTimes("Glic.Instance.LifetimeDuration.Max21Days",
                                lifetime, base::Milliseconds(1), base::Days(21),
                                50);
  base::UmaHistogramCounts100("Glic.Instance.TotalTabsBoundInLifetime",
                              GetEventCount(GlicInstanceEvent::kTabBound));
  base::UmaHistogramCounts100("Glic.Instance.MaxConcurrentlyBoundTabs",
                              max_concurrently_bound_tabs_);
  base::UmaHistogramCounts100("Glic.Instance.TurnCount",
                              GetEventCount(GlicInstanceEvent::kTurnCompleted));
  base::UmaHistogramCounts100("Glic.Instance.SessionCount", session_count_);

  InputModesUsed modes_used = InputModesUsed::kNone;
  if (!inputs_modes_used_.empty()) {
    if (inputs_modes_used_.size() == 2) {
      modes_used = InputModesUsed::kTextAndAudio;
    } else {
      modes_used = inputs_modes_used_.Has(mojom::WebClientMode::kAudio)
                       ? InputModesUsed::kOnlyAudio
                       : InputModesUsed::kOnlyText;
    }
  }
  base::UmaHistogramEnumeration("Glic.Instance.InputModesUsed", modes_used);
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
      base::UmaHistogramLongTimes("Glic.Instance.TimeSinceLastActive",
                                  time_since_last_active);
      base::UmaHistogramCustomTimes("Glic.Instance.TimeSinceLastActive.24H",
                                    time_since_last_active, base::Seconds(1),
                                    base::Hours(24), 50);
      base::UmaHistogramCustomTimes("Glic.Instance.TimeSinceLastActive.7D",
                                    time_since_last_active, base::Seconds(1),
                                    base::Days(7), 50);
    }
  }

  last_active_time_ = base::TimeTicks::Now();
  session_manager_.OnActivationChanged(is_active);
  activity_tracker_->OnStateChanged(is_active);
}

void GlicInstanceMetrics::OnVisibilityChanged(bool is_visible) {
  session_manager_.OnVisibilityChanged(is_visible);
  if (visibility_tracker_->state() && !is_visible) {
    OnInstanceHidden();
  }
  visibility_tracker_->OnStateChanged(is_visible);
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
    const std::optional<EmbedderKey>& key) {
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
  if (key.has_value()) {
    if (const auto* tab_ptr = std::get_if<tabs::TabInterface*>(&key.value())) {
      if (auto* helper = GlicInstanceHelper::From(*tab_ptr)) {
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
  if (side_panel_open_times_.contains(tab->GetHandle())) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kSidePanelOpenedWhileAlreadyOpen);
    return;
  }
  side_panel_open_times_[tab->GetHandle()] = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.SidePanel"));
  LogEvent(GlicInstanceEvent::kSidePanelShown);
  LogEvent(GlicInstanceEvent::kShown);
}

void GlicInstanceMetrics::OnShowInFloaty(const ShowOptions& options) {
  if (!floaty_open_time_.is_null()) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
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
      base::UmaHistogramEnumeration("Glic.Instance.Floaty.InitialMode",
                                    floaty_options->initial_mode);
    }
  }
  LogEvent(GlicInstanceEvent::kShown);
}

void GlicInstanceMetrics::OnFloatyClosed() {
  if (floaty_open_time_.is_null()) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kFloatyClosedWithoutOpen);
    return;
  }
  base::UmaHistogramCustomTimes("Glic.Instance.Floaty.OpenDuration",
                                base::TimeTicks::Now() - floaty_open_time_,
                                base::Milliseconds(1), base::Hours(1), 50);
}

void GlicInstanceMetrics::OnSidePanelClosed(tabs::TabInterface* tab) {
  if (!tab) {
    return;
  }

  if (auto* helper = GlicInstanceHelper::From(tab)) {
    helper->OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
  }

  tabs::TabHandle tab_handle = tab->GetHandle();
  auto it = side_panel_open_times_.find(tab_handle);
  if (it == side_panel_open_times_.end()) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kSidePanelClosedWithoutOpen);
    return;
  }

  base::UmaHistogramCustomTimes("Glic.Instance.SidePanel.OpenDuration",
                                base::TimeTicks::Now() - it->second,
                                base::Milliseconds(1), base::Hours(1), 50);
  side_panel_open_times_.erase(it);
}

void GlicInstanceMetrics::OnDetach() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Detach"));
  LogEvent(GlicInstanceEvent::kDetachedToFloaty);
}

void GlicInstanceMetrics::OnUnbindEmbedder(EmbedderKey key) {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.UnBind"));
  LogEvent(GlicInstanceEvent::kUnbindEmbedder);
  if (std::holds_alternative<tabs::TabInterface*>(key)) {
    if (auto* helper =
            GlicInstanceHelper::From(*std::get_if<tabs::TabInterface*>(&key))) {
      // Log NoAction if instance is unbound before any other actions occur.
      helper->OnDaisyChainAction(DaisyChainFirstAction::kNoAction);
    }
  }
  tabs::TabInterface** tab_ptr = std::get_if<tabs::TabInterface*>(&key);
  if (tab_ptr) {
    tabs::TabInterface* tab = *tab_ptr;
    tabs::TabHandle tab_handle = tab->GetHandle();
    auto it = side_panel_open_times_.find(tab_handle);
    if (it != side_panel_open_times_.end()) {
      base::UmaHistogramCustomTimes("Glic.Instance.SidePanel.OpenDuration",
                                    base::TimeTicks::Now() - it->second,
                                    base::Milliseconds(1), base::Hours(1), 50);
      side_panel_open_times_.erase(it);
    } else {
      base::UmaHistogramEnumeration(
          "Glic.Instance.Metrics.Error",
          GlicInstanceMetricsError::kTabUnbindWithoutOpen);
    }
    tab_depths_.erase(tab_handle);
    if (bound_tab_count_ > 0) {
      bound_tab_count_--;
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
        base::UmaHistogramExactLinear("Glic.Tab.DaisyChainDepth", depth, 50);
      }

      if (auto* new_helper = GlicInstanceHelper::From(new_tab)) {
        new_helper->SetIsDaisyChained();
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

void GlicInstanceMetrics::OnInstanceHidden() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Hide"));
  LogEvent(GlicInstanceEvent::kInstanceHidden);
}

void GlicInstanceMetrics::OnClose() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Close"));
  LogEvent(GlicInstanceEvent::kClose);
  base::UmaHistogramEnumeration("Glic.PanelWebUiState.FinishState3",
                                last_web_ui_state_);
}

void GlicInstanceMetrics::OnToggle(glic::mojom::InvocationSource source,
                                   const ShowOptions& options,
                                   bool is_showing) {
  if (!is_showing) {
    invocation_start_time_ = base::TimeTicks::Now();
    last_invocation_source_ = source;
  }
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Toggle"));
  if (std::holds_alternative<FloatingShowOptions>(options.embedder_options)) {
    base::UmaHistogramEnumeration("Glic.Instance.Floaty.ToggleSource", source);
  } else {
    base::UmaHistogramEnumeration("Glic.Instance.SidePanel.ToggleSource",
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

void GlicInstanceMetrics::OnWebUiStateChanged(mojom::WebUiState state) {
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
        base::UmaHistogramCustomTimes(
            base::StrCat({"Glic.Instance.WebUiLoadTime", visibility_suffix}),
            load_time, base::Milliseconds(1), base::Seconds(60), 50);
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
  }
}

void GlicInstanceMetrics::OnClientReady(EmbedderType type) {
  if (invocation_start_time_.is_null()) {
    return;
  }
  base::TimeDelta presentation_time =
      base::TimeTicks::Now() - invocation_start_time_;
  const char* suffix =
      (type == EmbedderType::kSidePanel) ? "SidePanel" : "Floaty";
  base::UmaHistogramCustomTimes(
      base::StrCat({"Glic.Instance.PanelPresentationTime.", suffix}),
      presentation_time, base::Milliseconds(1), base::Seconds(60), 50);
  invocation_start_time_ = base::TimeTicks();
}

void GlicInstanceMetrics::LogEvent(GlicInstanceEvent event) {
  base::UmaHistogramEnumeration("Glic.Instance.EventCounts", event);
  if (event_counts_[event] == 0) {
    // This is recorded only the first time an event occurs within this sessions
    // lifetime.
    base::UmaHistogramEnumeration("Glic.Instance.HadEvent", event);
  }
  event_counts_[event]++;

  session_manager_.OnEvent(event);
}

int GlicInstanceMetrics::GetEventCount(GlicInstanceEvent event) {
  const auto it = event_counts_.find(event);
  return it == event_counts_.end() ? 0 : it->second;
}

void GlicInstanceMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  // Try to attribute the input submission to the currently focused tab for
  // daisy chain metrics.
  if (current_ui_mode_ == EmbedderType::kSidePanel && sharing_manager_) {
    if (auto* tab = sharing_manager_->GetFocusedTabData().focus()) {
      if (auto* helper = GlicInstanceHelper::From(tab)) {
        helper->OnDaisyChainAction(DaisyChainFirstAction::kInputSubmitted);
      }
    }
  }

  if (turn_.response_started_) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kInputSubmittedWhileResponseInProgress);
    return;
  }
  if (!visibility_tracker_->state()) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kInputSubmittedWhileHidden);
    return;
  }
  session_manager_.OnUserInputSubmitted(mode);
  LogEvent(GlicInstanceEvent::kUserInputSubmitted);
  turn_.input_submitted_time_ = base::TimeTicks::Now();
  turn_.ui_mode_ = current_ui_mode_;
  turn_.input_mode_ = mode;
  input_mode_ = mode;
  inputs_modes_used_.Put(mode);
}

void GlicInstanceMetrics::DidRequestContextFromFocusedTab() {
  LogEvent(GlicInstanceEvent::kContextRequested);
  turn_.did_request_context_ = true;
}

void GlicInstanceMetrics::OnResponseStarted() {
  LogEvent(GlicInstanceEvent::kResponseStarted);
  turn_.response_started_ = true;

  // It doesn't make sense to record response start without input submission.
  if (turn_.input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kResponseStartWithoutInput);
    return;
  }

  if (!visibility_tracker_->state()) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kResponseStartWhileHidingOrHidden);
    return;
  }

  base::TimeDelta start_time =
      base::TimeTicks::Now() - turn_.input_submitted_time_;
  base::UmaHistogramMediumTimes("Glic.Turn.ResponseStartTime", start_time);
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Glic.Turn.ResponseStartTime.InputMode.", mode_string}),
      start_time);

  if (turn_.did_request_context_) {
    base::UmaHistogramMediumTimes("Glic.Turn.ResponseStartTime.WithContext",
                                  start_time);
  } else {
    base::UmaHistogramMediumTimes("Glic.Turn.ResponseStartTime.WithoutContext",
                                  start_time);
  }
}

void GlicInstanceMetrics::OnResponseStopped(mojom::ResponseStopCause cause) {
  LogEvent(GlicInstanceEvent::kResponseStopped);
  // The client may call "stopped" without "started" for very short responses.
  // We synthetically call it ourselves in this case.
  if (!turn_.input_submitted_time_.is_null() && !turn_.response_started_) {
    OnResponseStarted();
  }

  std::string_view cause_suffix;
  switch (cause) {
    case mojom::ResponseStopCause::kUser:
      cause_suffix = ".ByUser";
      break;
    case mojom::ResponseStopCause::kOther:
      cause_suffix = ".Other";
      break;
    case mojom::ResponseStopCause::kUnknown:
      cause_suffix = ".UnknownCause";
      break;
  }

  if (turn_.input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration(
        "Glic.Instance.Metrics.Error",
        GlicInstanceMetricsError::kResponseStopWithoutInput);
    base::UmaHistogramEnumeration(
        base::StrCat({"Glic.Instance.Metrics.Error", cause_suffix}),
        GlicInstanceMetricsError::kResponseStopWithoutInput);
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta latency = now - turn_.input_submitted_time_;
    base::UmaHistogramMediumTimes(
        base::StrCat({"Glic.Turn.ResponseStopTime", cause_suffix}), latency);
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
    case EmbedderType::kUnknown:
      turn_source = GlicTurnSource::kUnknown;
      break;
  }
  base::UmaHistogramEnumeration("Glic.Turn.Source", turn_source);
  base::UmaHistogramEnumeration("Glic.Turn.InvocationSource",
                                last_invocation_source_);
  // Reset the turn.
  last_turn_ = turn_;
  turn_ = {};
}

void GlicInstanceMetrics::OnTurnCompleted(mojom::WebClientModel model,
                                          base::TimeDelta duration) {
  LogEvent(GlicInstanceEvent::kTurnCompleted);
  base::UmaHistogramMediumTimes(model == mojom::WebClientModel::kActor
                                    ? "Glic.Turn.Duration.Actor"
                                    : "Glic.Turn.Duration.Default",
                                duration);
}

void GlicInstanceMetrics::OnUserResizeStarted(const gfx::Size& start_size) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.Floaty.UserResizeStarted"));
  base::UmaHistogramCounts10000("Glic.Instance.Floaty.UserResizeStarted.Width",
                                start_size.width());
  base::UmaHistogramCounts10000("Glic.Instance.Floaty.UserResizeStarted.Height",
                                start_size.height());
}

void GlicInstanceMetrics::OnUserResizeEnded(const gfx::Size& end_size) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.Floaty.UserResizeEnded"));
  base::UmaHistogramCounts10000("Glic.Instance.Floaty.UserResizeEnded.Width",
                                end_size.width());
  base::UmaHistogramCounts10000("Glic.Instance.Floaty.UserResizeEnded.Height",
                                end_size.height());
}

void GlicInstanceMetrics::OnReaction(
    mojom::MetricUserInputReactionType reaction_type) {
  LogEvent(GlicInstanceEvent::kReaction);
  if (last_turn_.input_submitted_time_.is_null() ||
      input_mode_ != mojom::WebClientMode::kText) {
    return;
  }

  switch (reaction_type) {
    case mojom::MetricUserInputReactionType::kUnknown:
      return;
    case mojom::MetricUserInputReactionType::kCanned:
      if (!last_turn_.reported_reaction_time_canned_) {
        base::UmaHistogramMediumTimes(
            "Glic.Turn.FirstReaction.Text.Canned.Time",
            base::TimeTicks::Now() - last_turn_.input_submitted_time_);
        last_turn_.reported_reaction_time_canned_ = true;
      }
      return;
    case mojom::MetricUserInputReactionType::kModel:
      if (!last_turn_.reported_reaction_time_modelled_) {
        base::UmaHistogramMediumTimes(
            "Glic.Turn.FirstReaction.Text.Modelled.Time",
            base::TimeTicks::Now() - last_turn_.input_submitted_time_);
        last_turn_.reported_reaction_time_modelled_ = true;
      }
      return;
  }
}

void GlicInstanceMetrics::OnSessionStarted() {
  session_count_++;

  // If `last_session_end_time_` is not null, we can record the time between
  // sessions.
  if (!last_session_end_time_.is_null()) {
    const base::TimeDelta time_between_sessions =
        base::TimeTicks::Now() - last_session_end_time_;
    base::UmaHistogramCustomTimes("Glic.Instance.TimeBetweenSessions.7D",
                                  time_between_sessions, base::Seconds(1),
                                  base::Days(7), 50);
    base::UmaHistogramCustomTimes("Glic.Instance.TimeBetweenSessions.24H",
                                  time_between_sessions, base::Seconds(1),
                                  base::Hours(24), 50);
  }
}

void GlicInstanceMetrics::OnSessionFinished() {
  last_session_end_time_ = base::TimeTicks::Now();
}

void GlicInstanceMetrics::RecordAttachedContextTabCount(int tab_count) {
  base::UmaHistogramExactLinear("Glic.Response.AttachedContextCount", tab_count,
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

  base::UmaHistogramMediumTimes(
      base::StrCat({"Glic.Turn.LatencyByAttachedTabCount.", tab_count_suffix}),
      latency);
}

int GlicInstanceMetrics::GetPinnedTabCount() const {
  return pinned_tab_count_;
}

}  // namespace glic
