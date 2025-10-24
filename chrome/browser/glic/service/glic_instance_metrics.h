// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_METRICS_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

struct ShowOptions;

enum class DaisyChainSource {
  kUnknown = 0,
  kGlicContents = 1,
  kTabContents = 2,
  kActorAddTab = 3,
  kMaxValue = kActorAddTab,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This enum should be kept in sync with GlicInstanceEvent in enums.xml. Each
// value is recorded at most once per instance.

// LINT.IfChange(GlicInstanceEvent)
enum class GlicInstanceEvent {
  kInstanceCreated = 0,
  kWarmedInstanceCreated = 1,
  kInstanceCreatedWithoutWarming = 2,
  kInstancePromoted = 3,
  kSidePanelShown = 4,
  kFloatyShown = 5,
  kDetachedToFloaty = 6,
  kTabBound = 7,
  kTabBoundViaDaisyChain = 8,
  kDaisyChainFailed = 9,
  kConversationSwitchedFromFloaty = 10,
  kConversationSwitchedFromSidePanel = 11,
  kConversationSwitchedToFloaty = 12,
  kConversationSwitchedToSidePanel = 13,
  kRegisterConversation = 14,
  kInstanceHidden = 15,
  kClose = 16,
  kToggle = 17,
  kBoundTabDestroyed = 18,
  kCreateTab = 19,
  kCreateTask = 20,
  kPerformActions = 21,
  kStopActorTask = 22,
  kPauseActorTask = 23,
  kResumeActorTask = 24,
  kInterruptActorTask = 25,
  kUninterruptActorTask = 26,
  kWebUiStateUninitialized = 27,
  kWebUiStateBeginLoad = 28,
  kWebUiStateShowLoading = 29,
  kWebUiStateHoldLoading = 30,
  kWebUiStateFinishLoading = 31,
  kWebUiStateError = 32,
  kWebUiStateOffline = 33,
  kWebUiStateUnavailable = 34,
  kWebUiStateReady = 35,
  kWebUiStateUnresponsive = 36,
  kWebUiStateSignIn = 37,
  kWebUiStateGuestError = 38,
  kWebUiStateDisabledByAdmin = 39,
  kMaxValue = kWebUiStateDisabledByAdmin,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicInstanceEvent)

// Tracks and logs lifecycle events for a single GlicInstance.
class GlicInstanceMetrics {
 public:
  GlicInstanceMetrics();
  ~GlicInstanceMetrics();

  GlicInstanceMetrics(const GlicInstanceMetrics&) = delete;
  GlicInstanceMetrics& operator=(const GlicInstanceMetrics&) = delete;

  // Called when GlicInstanceImpl is created.
  void OnInstanceCreated();

  // Called when GlicInstanceImpl is destroyed.
  void OnInstanceDestroyed();

  // Called when a GlicInstance is bound to a tab.
  void OnBind();

  // Called when a new warmed GlicInstance is created.
  void OnWarmedInstanceCreated();

  // Called when an instance is promoted for subsequent use.
  void OnInstancePromoted();

  // Called when an instance is created without warming.
  void OnInstanceCreatedWithoutWarming();

  // Called when this instance is shown in the side panel.
  void OnShowInSidePanel();

  // Called when this instance is shown in a floaty.
  void OnShowInFloaty();

  // Called when GlicInstanceImpl::SwitchConversation is called from this
  // instance (usually via 'start new chat' or re etn chats selection).
  void OnSwitchFromConversation(const ShowOptions& show_options);

  // Called when GlicInstanceImpl::SwitchConversation is called to activate this
  // instance (usually via 'start new chat' or recent chats selection).
  void OnSwitchToConversation(const ShowOptions& show_options);

  // Called when GlicInstanceImpl is detaching to a floaty.
  void OnDetach();

  // Called when daisy chaining occurs on the instance.
  void OnDaisyChain(DaisyChainSource source, bool success);

  // Called when GlicInstanceImpl::RegisterConversation is called.
  void OnRegisterConversation(const std::string& conversation_id);

  // Called when a GlicInstanceImpl is hidden.
  void OnInstanceHidden();

  // Called when Close is called on the instance.
  void OnClose();

  // Called when Toggle is called on the instance.
  void OnToggle();

  // Called when a tab that was bound to this instance is destroyed.
  void OnBoundTabDestroyed();

  // Called when GlicInstanceImpl::CreateTab is called.
  void OnCreateTab();

  // Called when GlicInstanceImpl::CreateTask is called.
  void OnCreateTask();

  // Called when GlicInstanceImpl::PerformActions is called.
  void OnPerformActions();

  // Called when GlicInstanceImpl::StopActorTask is called.
  void OnStopActorTask();

  // Called when GlicInstanceImpl::PauseActorTask is called.
  void OnPauseActorTask();

  // Called when GlicInstanceImpl::ResumeActorTask is called.
  void OnResumeActorTask();

  // Called when GlicInstanceImpl::InterruptActorTask is called.
  void InterruptActorTask();

  // Called when GlicInstanceImpl::UninterruptActorTask is called.
  void UninterruptActorTask();

  // Called when GlicInstanceImpl::WebUiStateChanged is called.
  void OnWebUiStateChanged(mojom::WebUiState state);

 private:
  // Stores counts for events to ensure they are only logged once per instance.
  struct GlicInstanceEventCounts {
    GlicInstanceEventCounts();

    // go/keep-sorted start
    int bound_tab_destroyed{};
    int close{};
    int conversation_switched_from_floaty{};
    int conversation_switched_from_side_panel{};
    int conversation_switched_to_floaty{};
    int conversation_switched_to_side_panel{};
    int create_tab{};
    int create_task{};
    int daisy_chain_failed{};
    int detached_to_floaty{};
    int floaty_shown{};
    int instance_created_without_warming{};
    int instance_created{};
    int instance_destroyed{};
    int instance_hidden{};
    int instance_promoted{};
    int interrupt_actor_task{};
    int pause_actor_task{};
    int perform_actions{};
    int register_conversation{};
    int resume_actor_task{};
    int side_panel_shown{};
    int stop_actor_task{};
    int tab_bound_via_daisy_chain{};
    int tab_bound{};
    int toggle{};
    int uninterrupt_actor_task{};
    int warmed_instance_created{};
    int web_ui_state_begin_load{};
    int web_ui_state_disabled_by_admin{};
    int web_ui_state_error{};
    int web_ui_state_finish_loading{};
    int web_ui_state_guest_error{};
    int web_ui_state_hold_loading{};
    int web_ui_state_offline{};
    int web_ui_state_ready{};
    int web_ui_state_show_loading{};
    int web_ui_state_sign_in{};
    int web_ui_state_unavailable{};
    int web_ui_state_uninitialized{};
    int web_ui_state_unresponsive{};
    // go/keep-sorted end
  };

  // Logs the given event to the EventTotals histogram, and if the count is 0,
  // also logs to the EventCounts histogram. Increments the counter.
  void LogEvent(GlicInstanceEvent event, int& event_counter);

  GlicInstanceEventCounts event_counts_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_METRICS_H_
