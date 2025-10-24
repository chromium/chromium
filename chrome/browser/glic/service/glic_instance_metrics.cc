// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_metrics.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/service/glic_ui_types.h"

namespace glic {

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

GlicInstanceMetrics::GlicInstanceEventCounts::GlicInstanceEventCounts() =
    default;

GlicInstanceMetrics::GlicInstanceMetrics() = default;

GlicInstanceMetrics::~GlicInstanceMetrics() {
  OnInstanceDestroyed();
}

void GlicInstanceMetrics::OnInstanceCreated() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Created"));
  LogEvent(GlicInstanceEvent::kInstanceCreated, event_counts_.instance_created);
}

void GlicInstanceMetrics::OnInstanceDestroyed() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Destroyed"));
}

void GlicInstanceMetrics::OnBind() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Bind"));
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

void GlicInstanceMetrics::OnShowInSidePanel() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.SidePanel"));
  LogEvent(GlicInstanceEvent::kSidePanelShown, event_counts_.side_panel_shown);
}

void GlicInstanceMetrics::OnShowInFloaty() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Show.Floaty"));
  LogEvent(GlicInstanceEvent::kFloatyShown, event_counts_.floaty_shown);
}

void GlicInstanceMetrics::OnDetach() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Detach"));
  LogEvent(GlicInstanceEvent::kDetachedToFloaty,
           event_counts_.detached_to_floaty);
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
}

void GlicInstanceMetrics::OnToggle() {
  base::RecordAction(base::UserMetricsAction("Glic.Instance.Toggle"));
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
  switch (state) {
    case mojom::WebUiState::kUninitialized:
      base::RecordAction(base::UserMetricsAction(
          "Glic.Instance.WebUiStateChanged.Uninitialized"));
      LogEvent(GlicInstanceEvent::kWebUiStateUninitialized,
               event_counts_.web_ui_state_uninitialized);
      break;
    case mojom::WebUiState::kBeginLoad:
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
    case mojom::WebUiState::kReady:
      base::RecordAction(
          base::UserMetricsAction("Glic.Instance.WebUiStateChanged.Ready"));
      LogEvent(GlicInstanceEvent::kWebUiStateReady,
               event_counts_.web_ui_state_ready);
      break;
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

void GlicInstanceMetrics::LogEvent(GlicInstanceEvent event,
                                   int& event_counter) {
  base::UmaHistogramEnumeration("Glic.Instance.EventTotals", event);
  if (event_counter == 0) {
    base::UmaHistogramEnumeration("Glic.Instance.EventCounts", event);
  }
  event_counter++;
}

}  // namespace glic
