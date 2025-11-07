// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_metrics.h"

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

}  // namespace

GlicInstanceMetrics::GlicInstanceEventCounts::GlicInstanceEventCounts() =
    default;

GlicInstanceMetrics::GlicInstanceMetrics() : session_manager_(this) {}

GlicInstanceMetrics::~GlicInstanceMetrics() {
  OnInstanceDestroyed();
}

void GlicInstanceMetrics::OnInstanceCreated() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Created"));
  creation_time_ = base::TimeTicks::Now();
  last_activation_change_time_ = creation_time_;
  last_visibility_change_time_ = creation_time_;
  LogEvent(GlicInstanceEvent::kInstanceCreated, event_counts_.instance_created);
}

void GlicInstanceMetrics::OnInstanceDestroyed() {
  session_manager_.OnOwnerDestroyed();

  base::RecordAction(base::UserMetricsAction("Glic.Instance.Destroyed"));

  // Add the time spent in the final state before destruction.
  if (is_active_) {
    OnActivationChanged(false);
  }
  if (is_visible_) {
    OnVisibilityChanged(false);
  }

  const base::TimeDelta lifetime = base::TimeTicks::Now() - creation_time_;
  const base::TimeDelta background_time = lifetime - total_active_time_;
  const base::TimeDelta hidden_time = lifetime - total_visible_time_;

  base::UmaHistogramCustomTimes("Glic.Instance.TotalActiveDuration",
                                total_active_time_, base::Milliseconds(1),
                                base::Hours(24), 50);
  base::UmaHistogramCustomTimes("Glic.Instance.TotalBackgroundDuration",
                                background_time, base::Milliseconds(1),
                                base::Hours(24), 50);
  base::UmaHistogramCustomTimes("Glic.Instance.TotalVisibleDuration",
                                total_visible_time_, base::Milliseconds(1),
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
                              event_counts_.tab_bound);
  base::UmaHistogramCounts100("Glic.Instance.MaxConcurrentlyBoundTabs",
                              max_concurrently_bound_tabs_);
  base::UmaHistogramCounts100("Glic.Instance.TurnCount",
                              event_counts_.turn_count);
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
  if (is_active == is_active_) {
    return;
  }

  session_manager_.OnActivationChanged(is_active);

  base::TimeDelta time_in_state =
      base::TimeTicks::Now() - last_activation_change_time_;
  // if is_active_ then activation changed to false.
  if (is_active_) {
    total_active_time_ += time_in_state;
    base::UmaHistogramCustomTimes("Glic.Instance.UninterruptedActiveDuration",
                                  time_in_state, base::Milliseconds(1),
                                  base::Hours(1), 50);
  }

  is_active_ = is_active;
  last_activation_change_time_ = base::TimeTicks::Now();
}

void GlicInstanceMetrics::OnVisibilityChanged(bool is_visible) {
  if (is_visible == is_visible_) {
    return;
  }

  session_manager_.OnVisibilityChanged(is_visible);

  base::TimeDelta time_in_state =
      base::TimeTicks::Now() - last_visibility_change_time_;
  // if is_visible_ then visibility changed to false.
  if (is_visible_) {
    OnInstanceHidden();
    total_visible_time_ += time_in_state;
    base::UmaHistogramCustomTimes("Glic.Instance.UninterruptedVisibleDuration",
                                  time_in_state, base::Milliseconds(1),
                                  base::Hours(1), 50);
  }

  is_visible_ = is_visible;
  last_visibility_change_time_ = base::TimeTicks::Now();
}

void GlicInstanceMetrics::OnBind() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Bind"));
  bound_tab_count_++;
  if (bound_tab_count_ > max_concurrently_bound_tabs_) {
    max_concurrently_bound_tabs_ = bound_tab_count_;
  }
  LogEvent(GlicInstanceEvent::kTabBound, event_counts_.tab_bound);
}

void GlicInstanceMetrics::OnInstancePromoted() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Promoted"));
  LogEvent(GlicInstanceEvent::kInstancePromoted,
           event_counts_.instance_promoted);
}

void GlicInstanceMetrics::OnWarmedInstanceCreated() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.CreatedWarmedInstance"));
  LogEvent(GlicInstanceEvent::kWarmedInstanceCreated,
           event_counts_.warmed_instance_created);
}

void GlicInstanceMetrics::OnInstanceCreatedWithoutWarming() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.CreatedInstanceWithoutWarming"));
  LogEvent(GlicInstanceEvent::kInstanceCreatedWithoutWarming,
           event_counts_.instance_created_without_warming);
}

void GlicInstanceMetrics::OnSwitchFromConversation(
    const ShowOptions& show_options) {
  if (std::holds_alternative<FloatingShowOptions>(
          show_options.embedder_options)) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.SwitchFromConversation.Floaty"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedFromFloaty,
             event_counts_.conversation_switched_from_floaty);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Glic.Instance.SwitchFromConversation.SidePanel"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedFromSidePanel,
             event_counts_.conversation_switched_from_side_panel);
  }
}

void GlicInstanceMetrics::OnSwitchToConversation(
    const ShowOptions& show_options) {
  if (std::holds_alternative<FloatingShowOptions>(
          show_options.embedder_options)) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.SwitchToConversation.Floaty"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedToFloaty,
             event_counts_.conversation_switched_to_floaty);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Glic.Instance.SwitchToConversation.SidePanel"));
    LogEvent(GlicInstanceEvent::kConversationSwitchedToSidePanel,
             event_counts_.conversation_switched_to_side_panel);
  }
}

void GlicInstanceMetrics::OnShowInSidePanel(tabs::TabInterface* tab) {
  if (!tab) {
    return;
  }
  side_panel_open_times_[tab->GetHandle().raw_value()] = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.SidePanel"));
  LogEvent(GlicInstanceEvent::kSidePanelShown, event_counts_.side_panel_shown);
}

void GlicInstanceMetrics::OnShowInFloaty() {
  floaty_open_time_ = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.Floaty"));
  LogEvent(GlicInstanceEvent::kFloatyShown, event_counts_.floaty_shown);
}

void GlicInstanceMetrics::OnFloatyClosed() {
  if (floaty_open_time_.is_null()) {
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
  int tab_id = tab->GetHandle().raw_value();
  auto it = side_panel_open_times_.find(tab_id);
  if (it == side_panel_open_times_.end()) {
    return;
  }

  base::UmaHistogramCustomTimes("Glic.Instance.SidePanel.OpenDuration",
                                base::TimeTicks::Now() - it->second,
                                base::Milliseconds(1), base::Hours(1), 50);
  side_panel_open_times_.erase(it);
}

void GlicInstanceMetrics::OnDetach() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Detach"));
  LogEvent(GlicInstanceEvent::kDetachedToFloaty,
           event_counts_.detached_to_floaty);
}

void GlicInstanceMetrics::OnUnbindEmbedder(EmbedderKey key) {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.UnBind"));
  LogEvent(GlicInstanceEvent::kUnbindEmbedder, event_counts_.unbind_embedder);

  auto* tab_ptr = std::get_if<tabs::TabInterface*>(&key);
  if (tab_ptr) {
    auto* tab = *tab_ptr;
    int tab_id = tab->GetHandle().raw_value();
    auto it = side_panel_open_times_.find(tab_id);
    if (it != side_panel_open_times_.end()) {
      base::UmaHistogramCustomTimes("Glic.Instance.SidePanel.OpenDuration",
                                    base::TimeTicks::Now() - it->second,
                                    base::Milliseconds(1), base::Hours(1), 50);
      side_panel_open_times_.erase(it);
    }
    if (bound_tab_count_ > 0) {
      bound_tab_count_--;
    }
  }
}

void GlicInstanceMetrics::OnDaisyChain(DaisyChainSource source, bool success) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"Glic.Instance.DaisyChain.",
                    GetDaisyChainSourceString(source), ".",
                    success ? "Success" : "Failure"})
          .c_str()));
  if (success) {
    LogEvent(GlicInstanceEvent::kTabBoundViaDaisyChain,
             event_counts_.tab_bound_via_daisy_chain);
  } else {
    LogEvent(GlicInstanceEvent::kDaisyChainFailed,
             event_counts_.daisy_chain_failed);
  }
}

void GlicInstanceMetrics::OnRegisterConversation(
    const std::string& conversation_id) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.RegisterConversation"));
  LogEvent(GlicInstanceEvent::kRegisterConversation,
           event_counts_.register_conversation);
}

void GlicInstanceMetrics::OnInstanceHidden() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Hide"));
  LogEvent(GlicInstanceEvent::kInstanceHidden, event_counts_.instance_hidden);
}

void GlicInstanceMetrics::OnClose() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Close"));
  LogEvent(GlicInstanceEvent::kClose, event_counts_.close);
  base::UmaHistogramEnumeration("Glic.PanelWebUiState.FinishState3",
                                last_web_ui_state_);
}

void GlicInstanceMetrics::OnToggle(glic::mojom::InvocationSource source,
                                   const ShowOptions& options,
                                   bool is_showing) {
  if (!is_showing) {
    invocation_start_time_ = base::TimeTicks::Now();
  }
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Toggle"));
  if (std::holds_alternative<FloatingShowOptions>(options.embedder_options)) {
    base::UmaHistogramEnumeration("Glic.Instance.Floaty.ToggleSource", source);
  } else {
    base::UmaHistogramEnumeration("Glic.Instance.SidePanel.ToggleSource",
                                  source);
  }
  LogEvent(GlicInstanceEvent::kToggle, event_counts_.toggle);
}

void GlicInstanceMetrics::OnBoundTabDestroyed() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.BoundTabDestroyed"));
  LogEvent(GlicInstanceEvent::kBoundTabDestroyed,
           event_counts_.bound_tab_destroyed);
}

void GlicInstanceMetrics::OnCreateTab() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.CreateTab"));
  LogEvent(GlicInstanceEvent::kCreateTab, event_counts_.create_tab);
}

void GlicInstanceMetrics::OnCreateTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.CreateTask"));
  LogEvent(GlicInstanceEvent::kCreateTask, event_counts_.create_task);
}

void GlicInstanceMetrics::OnPerformActions() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.PerformActions"));
  LogEvent(GlicInstanceEvent::kPerformActions, event_counts_.perform_actions);
}

void GlicInstanceMetrics::OnStopActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.StopActorTask"));
  LogEvent(GlicInstanceEvent::kStopActorTask, event_counts_.stop_actor_task);
}

void GlicInstanceMetrics::OnPauseActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.PauseActorTask"));
  LogEvent(GlicInstanceEvent::kPauseActorTask, event_counts_.pause_actor_task);
}

void GlicInstanceMetrics::OnResumeActorTask() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.ResumeActorTask"));
  LogEvent(GlicInstanceEvent::kResumeActorTask,
           event_counts_.resume_actor_task);
}

void GlicInstanceMetrics::InterruptActorTask() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.InterruptActorTask"));
  LogEvent(GlicInstanceEvent::kInterruptActorTask,
           event_counts_.interrupt_actor_task);
}

void GlicInstanceMetrics::UninterruptActorTask() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Instance.UninterruptActorTask"));
  LogEvent(GlicInstanceEvent::kUninterruptActorTask,
           event_counts_.uninterrupt_actor_task);
}

void GlicInstanceMetrics::OnWebUiStateChanged(mojom::WebUiState state) {
  last_web_ui_state_ = state;
  switch (state) {
    case mojom::WebUiState::kUninitialized:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Uninitialized"));
      LogEvent(GlicInstanceEvent::kWebUiStateUninitialized,
               event_counts_.web_ui_state_uninitialized);
      break;
    case mojom::WebUiState::kBeginLoad:
      web_ui_load_start_time_ = base::TimeTicks::Now();
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.BeginLoad"));
      LogEvent(GlicInstanceEvent::kWebUiStateBeginLoad,
               event_counts_.web_ui_state_begin_load);
      break;
    case mojom::WebUiState::kShowLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.ShowLoading"));
      LogEvent(GlicInstanceEvent::kWebUiStateShowLoading,
               event_counts_.web_ui_state_show_loading);
      break;
    case mojom::WebUiState::kHoldLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.HoldLoading"));
      LogEvent(GlicInstanceEvent::kWebUiStateHoldLoading,
               event_counts_.web_ui_state_hold_loading);
      break;
    case mojom::WebUiState::kFinishLoading:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.FinishLoading"));
      LogEvent(GlicInstanceEvent::kWebUiStateFinishLoading,
               event_counts_.web_ui_state_finish_loading);
      break;
    case mojom::WebUiState::kError:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Error"));
      LogEvent(GlicInstanceEvent::kWebUiStateError,
               event_counts_.web_ui_state_error);
      break;
    case mojom::WebUiState::kOffline:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Offline"));
      LogEvent(GlicInstanceEvent::kWebUiStateOffline,
               event_counts_.web_ui_state_offline);
      break;
    case mojom::WebUiState::kUnavailable:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Unavailable"));
      LogEvent(GlicInstanceEvent::kWebUiStateUnavailable,
               event_counts_.web_ui_state_unavailable);
      break;
    case mojom::WebUiState::kReady: {
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Ready"));
      LogEvent(GlicInstanceEvent::kWebUiStateReady,
               event_counts_.web_ui_state_ready);
      if (!web_ui_load_start_time_.is_null()) {
        base::TimeDelta load_time =
            base::TimeTicks::Now() - web_ui_load_start_time_;
        std::string_view visibility_suffix =
            is_visible_ ? ".Visible" : ".Nonvisible";
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
      LogEvent(GlicInstanceEvent::kWebUiStateUnresponsive,
               event_counts_.web_ui_state_unresponsive);
      break;
    case mojom::WebUiState::kSignIn:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.SignIn"));
      LogEvent(GlicInstanceEvent::kWebUiStateSignIn,
               event_counts_.web_ui_state_sign_in);
      break;
    case mojom::WebUiState::kGuestError:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.GuestError"));
      LogEvent(GlicInstanceEvent::kWebUiStateGuestError,
               event_counts_.web_ui_state_guest_error);
      break;
    case mojom::WebUiState::kDisabledByAdmin:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.DisabledByAdmin"));
      LogEvent(GlicInstanceEvent::kWebUiStateDisabledByAdmin,
               event_counts_.web_ui_state_disabled_by_admin);
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

void GlicInstanceMetrics::LogEvent(GlicInstanceEvent event,
                                   int& event_counter) {
  base::UmaHistogramEnumeration("Glic.Instance.EventCounts", event);
  if (event_counter == 0) {
    base::UmaHistogramEnumeration("Glic.Instance.HadEvent", event);
  }
  event_counter++;
}

void GlicInstanceMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  session_manager_.OnUserInputSubmitted(mode);
  LogEvent(GlicInstanceEvent::kUserInputSubmitted,
           event_counts_.user_input_submitted);
  turn_.input_submitted_time_ = base::TimeTicks::Now();
  input_mode_ = mode;
  inputs_modes_used_.Put(mode);
}

void GlicInstanceMetrics::DidRequestContextFromFocusedTab() {
  LogEvent(GlicInstanceEvent::kContextRequested,
           event_counts_.context_requested);
  turn_.did_request_context_ = true;
}

void GlicInstanceMetrics::OnResponseStarted() {
  LogEvent(GlicInstanceEvent::kResponseStarted, event_counts_.response_started);
  turn_.response_started_ = true;

  // It doesn't make sense to record response start without input submission.
  if (turn_.input_submitted_time_.is_null()) {
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
  LogEvent(GlicInstanceEvent::kResponseStopped, event_counts_.response_stopped);
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

  if (!turn_.input_submitted_time_.is_null()) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::UmaHistogramMediumTimes(
        base::StrCat({"Glic.Turn.ResponseStopTime", cause_suffix}),
        now - turn_.input_submitted_time_);
  }

  // Reset the turn.
  turn_ = {};
}

void GlicInstanceMetrics::OnTurnCompleted(mojom::WebClientModel model,
                                          base::TimeDelta duration) {
  session_manager_.OnTurnCompleted();

  LogEvent(GlicInstanceEvent::kTurnCompleted, event_counts_.turn_completed);
  event_counts_.turn_count++;
  base::UmaHistogramMediumTimes(model == mojom::WebClientModel::kActor
                                    ? "Glic.Turn.Duration.Actor"
                                    : "Glic.Turn.Duration.Default",
                                duration);
}

void GlicInstanceMetrics::OnReaction(
    mojom::MetricUserInputReactionType reaction_type) {
  LogEvent(GlicInstanceEvent::kReaction, event_counts_.reaction);
  if (turn_.input_submitted_time_.is_null() ||
      input_mode_ != mojom::WebClientMode::kText) {
    return;
  }

  switch (reaction_type) {
    case mojom::MetricUserInputReactionType::kUnknown:
      return;
    case mojom::MetricUserInputReactionType::kCanned:
      if (!turn_.reported_reaction_time_canned_) {
        base::UmaHistogramMediumTimes(
            "Glic.Turn.FirstReaction.Text.Canned.Time",
            base::TimeTicks::Now() - turn_.input_submitted_time_);
        turn_.reported_reaction_time_canned_ = true;
      }
      return;
    case mojom::MetricUserInputReactionType::kModel:
      if (!turn_.reported_reaction_time_modelled_) {
        base::UmaHistogramMediumTimes(
            "Glic.Turn.FirstReaction.Text.Modelled.Time",
            base::TimeTicks::Now() - turn_.input_submitted_time_);
        turn_.reported_reaction_time_modelled_ = true;
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

}  // namespace glic
