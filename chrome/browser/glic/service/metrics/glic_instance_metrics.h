// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/service/glic_state_tracker.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace base {
class TimeTicks;
class TimeDelta;
}  // namespace base

namespace glic {

class GlicSharingManager;
struct ShowOptions;

// This enumerates a set of possible lifecycle errors which are logged when the
// sequence of received events was not expected.
// LINT.IfChange(GlicInstanceMetricsError)
enum class GlicInstanceMetricsError {
  kResponseStartWithoutInput = 0,
  kResponseStopWithoutInput = 1,
  kResponseStartWhileHidingOrHidden = 2,
  kInputSubmittedWhileResponseInProgress = 3,
  kSidePanelOpenedWhileAlreadyOpen = 4,
  kFloatyOpenedWhileAlreadyOpen = 5,
  kInputSubmittedWhileHidden = 6,
  kTabUnbindWithoutOpen = 7,
  kSidePanelClosedWithoutOpen = 8,
  kFloatyClosedWithoutOpen = 9,
  kMaxValue = kFloatyClosedWithoutOpen,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicInstanceMetricsError)

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
  kUnbindEmbedder = 40,
  kUserInputSubmitted = 41,
  kContextRequested = 42,
  kResponseStarted = 43,
  kResponseStopped = 44,
  kTurnCompleted = 45,
  kReaction = 46,
  kShown = 47,
  kMaxValue = kShown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicInstanceEvent)

// Tracks and logs lifecycle events for a single GlicInstance.
class GlicInstanceMetrics {
 public:
  enum class EmbedderType {
    kUnknown,
    kSidePanel,
    kFloaty,
  };

  GlicInstanceMetrics();
  explicit GlicInstanceMetrics(GlicSharingManager* sharing_manager);
  ~GlicInstanceMetrics();

  GlicInstanceMetrics(const GlicInstanceMetrics&) = delete;
  GlicInstanceMetrics& operator=(const GlicInstanceMetrics&) = delete;

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
  void OnShowInSidePanel(tabs::TabInterface* tab);

  // Called when this instance is shown in a floaty.
  void OnShowInFloaty(const ShowOptions& options);

  // Called when the floaty is hidden.
  void OnFloatyClosed();

  // Called when the side panel is closed.
  void OnSidePanelClosed(tabs::TabInterface* tab);

  // Called when an embedder is unbound from this instance.
  void OnUnbindEmbedder(EmbedderKey key);

  // Called when GlicInstanceImpl::SwitchConversation is called from this
  // instance (usually via 'start new chat' or re etn chats selection).
  void OnSwitchFromConversation(const ShowOptions& show_options,
                                const std::optional<EmbedderKey>& key);

  // Called when GlicInstanceImpl::SwitchConversation is called to activate this
  // instance (usually via 'start new chat' or recent chats selection).
  void OnSwitchToConversation(const ShowOptions& show_options);

  // Called when GlicInstanceImpl is detaching to a floaty.
  void OnDetach();

  // Called when daisy chaining occurs on the instance.
  void OnDaisyChain(DaisyChainSource source,
                    bool success,
                    tabs::TabInterface* new_tab = nullptr,
                    tabs::TabInterface* source_tab = nullptr);

  // Called when GlicInstanceImpl::RegisterConversation is called.
  void OnRegisterConversation(const std::string& conversation_id);

  // Called when a GlicInstanceImpl is hidden.
  void OnInstanceHidden();

  // Called when the activation state of the instance changes.
  void OnActivationChanged(bool is_active);

  // Called when the visibility state of the instance changes.
  void OnVisibilityChanged(bool is_visible);

  // Called when Close is called on the instance.
  void OnClose();

  // Called when Toggle is called on the instance.
  void OnToggle(glic::mojom::InvocationSource source,
                const ShowOptions& options,
                bool is_showing);

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

  // Called when the client is ready to show.
  void OnClientReady(EmbedderType type);

  // Turn metrics.
  void OnUserInputSubmitted(mojom::WebClientMode mode);
  void DidRequestContextFromFocusedTab();
  void OnResponseStarted();
  void OnResponseStopped(mojom::ResponseStopCause cause);
  void OnTurnCompleted(mojom::WebClientModel model, base::TimeDelta duration);

  void OnUserResizeStarted(const gfx::Size& start_size);
  void OnUserResizeEnded(const gfx::Size& end_size);

  void OnReaction(mojom::MetricUserInputReactionType reaction_type);

  // Records the number of tabs attached as context for a Glic response.
  void RecordAttachedContextTabCount(int tab_count);

  int GetPinnedTabCount() const;

  bool is_active() const {
    return activity_tracker_ ? activity_tracker_->state() : false;
  }

  GlicMetricsSessionManager& session_manager() { return session_manager_; }

 private:
  friend class GlicMetricsSessionManager;

  // Stores info scoped to the current turn. These members are cleared in
  // OnResponseStopped.
  struct TurnInfo {
    base::TimeTicks input_submitted_time_;
    // Set to true in OnResponseStarted() and set to false in
    // OnResponseStopped(). This is a workaround copied from GlicMetrics and
    // should be removed, see crbug.com/399151164.
    bool response_started_ = false;
    bool did_request_context_ = false;
    bool reported_reaction_time_canned_ = false;
    bool reported_reaction_time_modelled_ = false;
    EmbedderType ui_mode_ = EmbedderType::kUnknown;
    mojom::WebClientMode input_mode_ = mojom::WebClientMode::kUnknown;
  };

  // Logs the given event to the EventTotals histogram, and if the count is 0,
  // also logs to the EventCounts histogram. Increments the counter.
  void LogEvent(GlicInstanceEvent event);
  int GetEventCount(GlicInstanceEvent event);

  // Called by the session manager when it starts and ends.
  void OnSessionStarted();
  void OnSessionFinished();

  void OnPinnedTabsChanged(
      const std::vector<content::WebContents*>& pinned_contents);

  // Records the response latency (from user input submitted to response stop)
  // by the number of attached tabs.
  void RecordResponseLatencyByAttachedTabCount(base::TimeDelta latency);

  base::flat_map<GlicInstanceEvent, int> event_counts_;
  EmbedderType current_ui_mode_ = EmbedderType::kUnknown;

  // Keeps track of the current number of bound tabs to this instance.
  // Incremented in OnBind and decremented in OnUnbindEmbedder.
  int bound_tab_count_ = 0;
  // Stores the max bound_tab_count_ value during the instances lifetime.
  int max_concurrently_bound_tabs_ = 0;

  TurnInfo turn_;
  TurnInfo last_turn_;
  mojom::WebClientMode input_mode_ = mojom::WebClientMode::kUnknown;
  base::EnumSet<mojom::WebClientMode,
                mojom::WebClientMode::kMinValue,
                mojom::WebClientMode::kMaxValue>
      inputs_modes_used_;

  // The last web ui state received.
  mojom::WebUiState last_web_ui_state_ = mojom::WebUiState::kUninitialized;
  // The last invocation source that was used to show the panel.
  mojom::InvocationSource last_invocation_source_ =
      mojom::InvocationSource::kUnsupported;
  // Timestamp of last show start.
  base::TimeTicks invocation_start_time_;
  base::TimeTicks web_ui_load_start_time_;

  base::TimeTicks last_active_time_;
  bool is_active_ = false;
  base::TimeTicks creation_time_;
  base::TimeTicks floaty_open_time_;
  std::map<tabs::TabHandle, base::TimeTicks> side_panel_open_times_;

  std::unique_ptr<GlicStateTracker> activity_tracker_;
  std::unique_ptr<GlicStateTracker> visibility_tracker_;

  GlicMetricsSessionManager session_manager_;
  base::TimeTicks last_session_end_time_;
  int session_count_ = 0;
  int pinned_tab_count_ = 0;

  std::map<tabs::TabHandle, int> tab_depths_;

  base::CallbackListSubscription pinned_tabs_changed_subscription_;
  raw_ptr<GlicSharingManager> sharing_manager_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_
