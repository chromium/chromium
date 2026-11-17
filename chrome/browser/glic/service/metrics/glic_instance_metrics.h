// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/public/glic_instance_metrics_backwards_compatibility.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/service/glic_state_tracker.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"

class PrefService;
class Profile;

namespace metrics {

class ProfileMetricsService;
}

namespace tabs {
class TabInterface;
}

namespace enterprise_reporting {
class SaasUsageReportingController;
}

namespace base {
class TimeTicks;
class TimeDelta;
}  // namespace base

namespace glic {

class GlicSharingManagerInternal;
struct ShowOptions;

using SafeEmbedderKey = std::variant<tabs::TabHandle, FloatingEmbedderKey>;

// Tracks and logs lifecycle events for a single GlicInstance.
class GlicInstanceMetrics : public GlicInstanceMetricsBackwardsCompatibility {
 public:
  explicit GlicInstanceMetrics(
      const metrics::ProfileMetricsService* profile_metrics_service,
      Profile* profile);
  GlicInstanceMetrics(
      const metrics::ProfileMetricsService* profile_metrics_service,
      GlicSharingManagerInternal* sharing_manager,
      enterprise_reporting::SaasUsageReportingController*
          saas_usage_reporting_controller,
      Profile* profile);
  ~GlicInstanceMetrics() override;

  GlicInstanceMetrics(const GlicInstanceMetrics&) = delete;
  GlicInstanceMetrics& operator=(const GlicInstanceMetrics&) = delete;

  // `GlicInstanceMetricsBackwardsCompatibility`:
  void OnUserInputSubmitted(mojom::WebClientMode mode,
                            mojom::PromptType prompt_type) override;

  void OnResponseStarted();
  void OnResponseStopped(mojom::ResponseStopCause cause);
  void DidRequestContextFromFocusedTab();
  void DidRequestContextFromTab();
  void OnTurnCompleted(mojom::WebClientModel model, base::TimeDelta duration);
  void OnReaction(mojom::MetricUserInputReactionType reaction_type);
  void OnInputFocused();
  void OnGlicScrollAttempt();
  void OnGlicScrollComplete(bool success);

  // Called when the opt-in CTA is shown.
  void OnOptinImpression();

  // Called when the user accepted the FRE from this instance, either from its
  // web client (SetOnboardingCompleted) or from the native FRE
  // (GlicInstanceImpl::AcceptNativeFre).
  void OnOnboardingCompleted();

  // Called when the user explicitly declined the FRE ("No thanks") in this
  // instance's web client. The client closes the panel afterwards, which
  // without this call would be indistinguishable from closing it undecided.
  void OnOptinRejected();

  // Called when the web client holds a submitted prompt until the user
  // completes the FRE (message-first onboarding). The client reports the
  // prompt through OnUserInputSubmitted() only after consent, so this is what
  // marks that turn as submitted pre-consent.
  void OnUserInputHeldForOnboarding();

  // How an FRE attempt ended. Mirrored by GlicFreDecision in
  // //tools/metrics/structured/sync/structured.xml and translated by an
  // exhaustive switch in glic_instance_metrics.cc.
  // LINT.IfChange(FreDecision)
  enum class FreDecision {
    // OnOnboardingCompleted() from this instance's web client or native FRE.
    kAccepted = 0,
    // OnOptinRejected() from this instance's web client ("No thanks").
    kNoThanks = 1,
    // The active panel was closed with the FRE still pending.
    kPanelClosed = 2,
    // The instance was destroyed with the FRE still pending and without an
    // active embedder close being reported first (e.g. tearing down an
    // instance with a backgrounded side-panel FRE, or as a backstop if
    // destroyed without closing its embedders).
    kInstanceDestroyed = 3,
    // The panel was closed, or the instance destroyed, after the profile
    // consented through some other instance or surface.
    kAcceptedElsewhere = 4,
  };
  // LINT.ThenChange(//tools/metrics/structured/sync/structured.xml:GlicFreDecision)

  // How a turn ended. Mirrored by GlicTurnEnd in
  // //tools/metrics/structured/sync/structured.xml and translated by an
  // exhaustive switch in glic_instance_metrics.cc.
  // LINT.IfChange(TurnEnd)
  enum class TurnEnd {
    // OnResponseStopped() without a cause: the response completed.
    kResponseCompleted = 0,
    // OnResponseStopped() with ResponseStopCause::kUser.
    kStoppedByUser = 1,
    // OnResponseStopped() with ResponseStopCause::kOther: the client
    // stopped it, e.g. on an error.
    kStoppedOther = 2,
    // Another input was submitted before any response was reported for
    // this turn, which the client never reported as stopped.
    kSuperseded = 3,
    // The instance was destroyed before any response was reported.
    kInstanceDestroyed = 4,
  };
  // LINT.ThenChange(//tools/metrics/structured/sync/structured.xml:GlicTurnEnd)

  // Multi-tab tooltip metrics.
  void OnTooltipImpression(mojom::TooltipImpressionType impression);
  void OnTooltipHidden();

  // TODO(crbug.com/545714879): Remove OptInShownCallback once
  // OnFreOptInShown is logged directly or via direct profile helpers instead of
  // bubbling up to GlicOnboardingTracker.
  using OptInShownCallback = base::RepeatingCallback<void(ukm::SourceId)>;
  void SetOptInShownCallback(OptInShownCallback callback) {
    opt_in_shown_callback_ = std::move(callback);
  }

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

  enum class CloseReason { kExplicitlyClosed, kTabSwitched };

  // Called when the side panel is closed.
  void OnSidePanelClosed(tabs::TabInterface* tab, CloseReason reason);

  // Called when an embedder is unbound from this instance.
  void OnUnbindEmbedder(EmbedderKey key);

  // Called when GlicInstanceImpl::SwitchConversation is called from this
  // instance (usually via 'start new chat' or re etn chats selection).
  void OnSwitchFromConversation(const ShowOptions& show_options,
                                std::optional<EmbedderKey> active_key);

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

  // Called when an embedder for the instance is closed. `is_active_embedder`
  // is false when closing an inactive embedder or the old embedder while
  // moving to another surface (detach/attach), where the UI remains open.
  void OnClose(bool is_active_embedder = true);

  // Called when Toggle is called on the instance.
  void OnToggle(glic::mojom::InvocationSource source,
                const EmbedderKey& embedder_key,
                bool is_showing);

  // Sets `initial_invocation_source_` if it has not been set yet.
  void MaybeSetInitialInvocationSource(mojom::InvocationSource source);

  // Called before showing the instance to resolve any unseen completed response
  // and record return source if the instance has conversation history.
  void OnWillShow(const ShowOptions& options, bool has_conversation_id);

  // Called when the UI is shown and it was not already showing for this
  // instance.
  void OnOpen(glic::mojom::InvocationSource source, const ShowOptions& options);

  bool MarkShownAndCheckIfFirstTime(EmbedderKey key);

  // Clears the shown state for `key` and returns true if `key` was previously
  // marked as shown.
  bool ResetShownState(EmbedderKey key);

  // Called when a tab that was bound to this instance is destroyed.
  void OnBoundTabDestroyed(tabs::TabInterface* tab, bool is_active_embedder);

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

  // Called when GlicInstanceImpl::UninterruptActorTask is called.
  void UninterruptActorTask();

  // Called when GlicInstanceImpl::InterruptActorTask is called.
  void InterruptActorTask();

  // Called when GlicInstanceImpl::WebUiStateChanged is called.
  void OnWebUiStateChanged(mojom::WebUiState state);

  // Called when the client failed to become usable. Records the
  // Glic.ClientError structured metrics event, joined to the in-flight
  // invocation if there is one.
  void OnClientLoadError(ClientLoadErrorReason reason);

  // Called when the client is ready to show.
  void OnClientReady();

  // Called when the web client is cleared (reload, crash, navigation, or
  // hibernation).
  void OnWebClientCleared();

  // Updates metrics state when the user accepts the native FRE.
  void OnNativeFreAccepted();

  // Updates metrics state when the user accepts the WebUI FRE.
  void OnWebFreAccepted();

  // Guest readiness milestones for `Glic.Instance.TimeToActionable` and
  // `Glic.Instance.TimeToSubmittable`, reported by `GlicWebClientManager`.
  enum class GuestMilestone : size_t {
    kActionable = 0,
    kSubmittable = 1,
    kMaxValue = kSubmittable,
  };
  static constexpr size_t kGuestMilestoneCount =
      static_cast<size_t>(GuestMilestone::kMaxValue) + 1;

  // Called when the guest web client records the custom `performance.mark()`
  // corresponding to `milestone`.
  void OnGuestMilestoneMarkObserved(GuestMilestone milestone,
                                    base::TimeTicks mark_time);

  // Called when a mounted guest has submitted a compositor frame reflecting
  // `milestone`. If the panel is hidden at that point (e.g. during the hide
  // debounce), the sample is recorded when the panel becomes visible again
  // within the same invocation.
  void OnGuestMilestoneFrameSubmitted(GuestMilestone milestone);

  // Returns whether the current guest document still needs a compositor frame
  // callback for `milestone`.
  bool IsWaitingForGuestMilestone(GuestMilestone milestone) const;

  // Called when a guest is attached to the Host, or when the attached guest
  // crashes or commits a new primary main-frame navigation.
  void OnGuestReset();

  void OnUserResizeStarted(const gfx::Size& start_size);
  void OnUserResizeEnded(const gfx::Size& end_size);

  void OnZoomLevelChange();

  // Sets the input mode of the web client. Called when the web client reports
  // its mode as the panel opens, and on every subsequent mode change.
  void SetWebClientMode(mojom::WebClientMode mode);

  // Logs errors that occurred while trying to get context or image bytes from
  // a tab.
  void LogGetContextFromFocusedTabError(GlicGetContextFromTabError error);
  void LogGetContextFromTabError(GlicGetContextFromTabError error);
  void LogGetContextForActorFromTabError(GlicGetContextFromTabError error);
  void LogGetImageBytesFromTabError(GlicGetContextFromTabError error);

  // Events from the web client / instance UI.
  void OnContextUploadStarted();
  void OnContextUploadCompleted();
  void OnSessionTerminated();
  void OnResponseRated(bool positive);
  void LogClosedCaptionsShown();
  void OnActivateTabFromInstance(tabs::TabInterface* tab);

  // Records the number of tabs attached as context for a Glic response.
  void RecordAttachedContextTabCount(int tab_count);

  void RecordTabPinningStatusEvent(tabs::TabInterface* tab,
                                   GlicPinningStatusEvent event);

  enum class PendingImpression {
    kOptIn = 0,
  };

  // Routes skills WebUI actions from the frontend to their respective
  // metrics funnels.
  void RecordSkillsWebClientEvent(mojom::SkillsWebClientEvent action);

  // Called when the web client sends a browser actuation result over the
  // network.
  void OnActionSubmitted(bool is_retry);

  int GetPinnedTabCount() const;

  bool is_active() const {
    return activity_tracker_ ? activity_tracker_->state() : false;
  }

  GlicMetricsSessionManager& session_manager() { return session_manager_; }

  void SetActiveInvocationId(std::optional<uint64_t> invocation_id);

  std::optional<mojom::InvocationSource> initial_invocation_source() const {
    return initial_invocation_source_;
  }

  base::WeakPtr<GlicInstanceMetrics> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class GlicMetricsSessionManager;
  friend class GlicInstanceMetricsTest;

  // Stores info scoped to the current turn. These members are cleared in
  // OnResponseStopped.
  struct TurnInfo {
    TurnInfo();
    ~TurnInfo();

    base::TimeTicks input_submitted_time_;
    base::TimeTicks action_result_submitted_time_;
    // Set to true in OnResponseStarted() and set to false in
    // OnResponseStopped(). This is a workaround copied from GlicMetrics and
    // should be removed, see crbug.com/399151164.
    bool response_started_ = false;
    // When OnResponseStarted() was first called for this turn; null until
    // then.
    base::TimeTicks response_started_time_;
    bool did_request_context_ = false;
    EmbedderType ui_mode_ = EmbedderType::kUnknown;
    mojom::WebClientMode input_mode_ = mojom::WebClientMode::kUnknown;
    mojom::PromptType prompt_type_ = mojom::PromptType::kUnspecified;
    bool pending_scroll_complete_ = false;
    ukm::SourceId chosen_source_id_ = ukm::NoURLSourceId();
    // 1-based position of this turn among the instance's submitted turns;
    // 0 while no input has been submitted.
    int seq_ = 0;
    // Whether the user submitted the input before consenting: either the
    // profile had not consented when it was submitted (inline FRE arms) or
    // the web client held it until consent (message-first arms, reported via
    // OnUserInputHeldForOnboarding()).
    bool pre_consent_ = false;
    // The invocation that opened or most recently invoked this open of the
    // panel when the input was submitted, if any.
    std::optional<uint64_t> invocation_id_;
  };

  // Records the TurnSubmitted structured event for `turn_`.
  void RecordTurnSubmitted();
  // Records the TurnTerminated structured event for `turn_` if it has
  // submitted input that has not terminated yet.
  void RecordTurnTerminated(TurnEnd end);
  // Records per-turn response-start histograms, actions, and UKM. Called from
  // OnResponseStarted() and synthetically from OnResponseStopped() when a turn
  // ends without a prior OnResponseStarted() call.
  void RecordResponseStarted();

  // Logs the given event to the EventTotals histogram, and if the count is 0,
  // also logs to the EventCounts histogram. Increments the counter.
  void LogEvent(GlicInstanceEvent event);
  int GetEventCount(GlicInstanceEvent event);

  // Called by the session manager when it starts and ends.
  void OnSessionStarted();
  void OnSessionFinished();

  void OnPinnedTabsChanged(
      const std::vector<raw_ptr<tabs::TabInterface>>& pinned_tabs);

  // Records the response latency (from user input submitted to response stop)
  // by the number of attached tabs.
  void RecordResponseLatencyByAttachedTabCount(base::TimeDelta latency);

  // Records whether a multi-tab prompt was submitted after a tooltip
  // impression.
  void OnTooltipMultiTabPromptSubmitted();

  // Records ClosedWhileShown and PanelCloseWithin10s if a multi-tab tooltip was
  // shown since the last close or web-client load, then clears tooltip close
  // state so each episode counts at most once.
  void RecordTooltipClosedMetrics();

  void RecordSkillsInvokeFunnelStep(SkillsInvokeFunnel invoke_funnel);
  void RecordAndResetAutoOpenPdfMetric();
  void RecordOptInImpression();
  void MaybeRecordOptInImpression();

  // An FRE attempt: one open of this instance while the profile has not
  // consented, keyed by the invocation that opened it. It is the unit the
  // FreImpression and FreDecision structured events describe, and it is
  // instance scoped so that several unconsented instances (or a warm-pool
  // instance that is never shown) cannot be mistaken for one another.
  struct FreAttempt {
    // The invocation in flight when the attempt started, if any. Kept after
    // the invocation terminates, since a toggle invocation completes as soon
    // as the panel opens while the user is still looking at the FRE.
    std::optional<uint64_t> invocation_id;
    mojom::InvocationSource source = mojom::InvocationSource::kUnsupported;
    base::TimeTicks start_time;
    bool impression_recorded = false;
  };

  // Starts an FRE attempt on open while unconsented.
  void MaybeStartFreAttempt();
  // Records the FreImpression event for the attempt in flight once the guest
  // has reported the opt-in impression and the panel is visible with the
  // client ready. The guest reports the impression once per load, so this
  // also covers an attempt that reopens the same guest.
  void MaybeRecordFreImpression();
  // Records the FreImpression event for the attempt in flight, once. Called
  // directly when the native FRE card is shown, which needs no web client.
  void RecordFreImpression();
  // Records a FreDecision event and ends the attempt. Decisions the user
  // made (kAccepted, kNoThanks) are recorded even without an attempt in
  // flight; the others only end one, and become kAcceptedElsewhere if the
  // profile has consented through another instance or surface by then.
  void RecordFreDecision(FreDecision decision);

  // Records the duration and prompt count for the first time the side panel is
  // closed or the tab is switched.
  void MaybeRecordFirstSidePanelOpenMetrics(base::TimeDelta duration);

  // Records the duration the user waited before closing/dismissing the panel
  // while the client was still loading.
  void MaybeRecordTimeToDismissWhileLoading();

  // Stores info scoped to the current invocation loading phase.
  struct InvocationLoadState {
    base::TimeTicks start_time;
    EmbedderType embedder_type = EmbedderType::kUnknown;
    bool has_logged_dismiss_while_loading = false;
  };

  // Stores per-milestone timing state scoped to an active open/invocation.
  struct MilestoneInvocationState {
    base::TimeTicks start_time;
    EmbedderType embedder_type = EmbedderType::kUnknown;
    bool started_before_consent = false;
    // Whether a frame was submitted while an invocation was being timed but the
    // panel was hidden. Recorded when the panel is shown again.
    bool frame_submitted_while_hidden = false;
  };

  // Stores presentation state for a guest readiness milestone on the currently
  // loaded guest document. Reset on guest replacement, navigation commit,
  // crash, or hibernate.
  struct GuestMilestoneState {
    // Whether the active/inactive histogram has been resolved for this guest
    // document. Reloads while the panel is open aren't timed, but still set
    // this so reopening the reloaded guest doesn't log a bogus sample.
    // TODO(b/571994634): Track reload presentation time.
    bool recorded = false;
    // Whether this guest document has submitted a compositor frame after the
    // milestone mark.
    bool guest_frame_submitted = false;
    bool mark_observed = false;
  };

  struct GuestMilestoneSlot {
    GuestMilestoneState guest_state;
    MilestoneInvocationState invocation_state;
  };

  GuestMilestoneSlot& MilestoneSlot(GuestMilestone milestone) {
    return guest_milestones_[static_cast<size_t>(milestone)];
  }
  const GuestMilestoneSlot& MilestoneSlot(GuestMilestone milestone) const {
    return guest_milestones_[static_cast<size_t>(milestone)];
  }

  void RecordPanelPresentationTime(base::TimeTicks end_time);
  void RecordMilestoneTime(GuestMilestone milestone, base::TimeTicks end_time);
  void MaybeRecordFrameSubmittedWhileHidden(GuestMilestone milestone,
                                            base::TimeTicks now);
  void UpdateNativeFreStateForShow(const ShowOptions& options);
  void MaybeRecordAcceptToChatActionable(base::TimeTicks now);
  void CancelNativeFreWait();
  bool CanRecordOptInImpression() const;

  bool IsVisible() const;

  // Tracks a response that has finished streaming while the panel was
  // hidden or in an inactive window, pending resolution when the panel
  // becomes showing or active (or finalization if the instance is
  // destroyed first).
  struct PendingCompletedResponse {
    base::TimeTicks completion_time;
    bool is_actor = false;
    bool was_showing = false;
    std::optional<mojom::InvocationSource> return_source;
  };

  void RecordTurnShowingOutcome(bool is_actor, GlicTurnShowingOutcome outcome);
  void RecordCompletedWhileHiddenShown(bool is_actor,
                                       mojom::InvocationSource return_source,
                                       base::TimeDelta time_to_show);
  void RecordCompletedWhileHiddenDestroyed(
      bool is_actor,
      base::TimeDelta time_until_destroyed);
  void MaybeResolvePendingResponse();
  void FinalizePendingResponse(base::TimeTicks now);

  enum class NativeFreState {
    kNone,
    kWaitingForAccept,
    kAccepted,
  };

  base::flat_map<GlicInstanceEvent, int> event_counts_;
  EmbedderType current_ui_mode_ = EmbedderType::kUnknown;

  // Keeps track of the current number of bound tabs to this instance.
  // Incremented in OnBind and decremented in OnUnbindEmbedder.
  int bound_tab_count_ = 0;
  // Stores the max bound_tab_count_ value during the instances lifetime.
  int max_concurrently_bound_tabs_ = 0;

  TurnInfo turn_;
  TurnInfo last_turn_;
  // Number of turns submitted to this instance so far.
  int turn_count_ = 0;
  mojom::WebClientMode input_mode_ = mojom::WebClientMode::kUnknown;
  // The current web client input mode, as last reported via
  // SetWebClientMode() (reset to kUnknown in OnWebClientCleared()). Kept
  // separate from `input_mode_` so panel-open and mid-turn mode updates do
  // not alter turn metrics; used to suffix Glic.Api.GetContext*.Error
  // histograms.
  mojom::WebClientMode web_client_mode_ = mojom::WebClientMode::kUnknown;
  base::EnumSet<mojom::WebClientMode> inputs_modes_used_;

  // The last web ui state received.
  mojom::WebUiState last_web_ui_state_ = mojom::WebUiState::kUninitialized;
  // The last invocation source that was used to show the panel.
  mojom::InvocationSource last_invocation_source_ =
      mojom::InvocationSource::kUnsupported;
  std::optional<PendingCompletedResponse> pending_completed_response_;
  std::optional<uint64_t> active_invocation_id_ = std::nullopt;
  // The invocation that opened or most recently invoked the current open of
  // this instance; survives after a toggle invocation completes until OnClose.
  std::optional<uint64_t> open_invocation_id_ = std::nullopt;
  // When `active_invocation_id_` was set. Null when there is no invocation in
  // flight. Used to report how far into an invocation a failure happened.
  base::TimeTicks active_invocation_start_time_;
  std::optional<mojom::InvocationSource> initial_invocation_source_ =
      std::nullopt;
  bool did_open_ = false;
  InvocationLoadState invocation_load_state_;
  std::array<GuestMilestoneSlot, kGuestMilestoneCount> guest_milestones_{};
  base::TimeTicks web_ui_load_start_time_;
  base::TimeTicks client_ready_time_;
  base::TimeTicks open_start_time_;
  bool was_client_ready_on_open_ = false;
  bool input_focused_for_current_open_ = false;
  bool input_focused_for_current_invocation_ = false;
  bool input_focused_after_client_ready_ = false;

  base::TimeTicks last_active_time_;
  bool is_active_ = false;
  base::TimeTicks creation_time_;
  base::TimeTicks floaty_open_time_;
  std::map<tabs::TabHandle, base::TimeTicks> side_panel_open_times_;
  std::vector<tabs::TabHandle> tabs_with_side_panel_;
  ukm::SourceId auto_open_pdf_source_id_ = ukm::kInvalidSourceId;
  base::TimeTicks auto_open_pdf_start_time_;

  std::unique_ptr<GlicStateTracker> activity_tracker_;
  std::unique_ptr<GlicStateTracker> visibility_tracker_;

  // Must be declared before `session_manager_`, which copies it.
  GlicMetricReporter metric_reporter_;
  GlicMetricsSessionManager session_manager_;
  base::TimeTicks last_session_end_time_;
  int session_count_ = 0;
  int pinned_tab_count_ = 0;

  std::map<tabs::TabHandle, int> tab_depths_;

  bool is_client_ready_ = false;
  bool is_opt_in_pending_ = false;
  bool opt_in_impression_recorded_for_current_open_ = false;
  NativeFreState native_fre_state_ = NativeFreState::kNone;
  base::TimeTicks fre_accept_to_actionable_start_time_;
  // Whether the current guest document has reported the opt-in impression.
  // The web client reports it once per load, so it outlives a close and
  // reopen of the same guest; reset in OnGuestReset().
  bool guest_reported_opt_in_impression_ = false;
  std::optional<FreAttempt> fre_attempt_;
  bool has_consented_ = false;
  // Set by OnUserInputHeldForOnboarding(); consumed by the next
  // OnUserInputSubmitted() on this open, cleared when the panel closes.
  bool held_input_pending_ = false;
  OptInShownCallback opt_in_shown_callback_;

  base::CallbackListSubscription pinned_tabs_changed_subscription_;
  base::CallbackListSubscription tab_pinning_status_subscription_;
  const raw_ref<const metrics::ProfileMetricsService> profile_metrics_service_;
  raw_ptr<GlicSharingManagerInternal> sharing_manager_ = nullptr;
  raw_ptr<enterprise_reporting::SaasUsageReportingController>
      saas_usage_reporting_controller_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;

  bool first_side_panel_close_recorded_ = false;
  bool first_floaty_close_recorded_ = false;
  bool saas_usage_recorded_ = false;

  // The following variables are used for recording scroll related metrics.
  //
  // The number of scroll attempts (tracked per session and reset when the
  // session ends).
  int scroll_attempt_count_ = 0;

  // The number of zoom change attempts (tracked per instance and reset when
  // the instance is destroyed).
  int zoom_change_count_ = 0;

  base::flat_set<SafeEmbedderKey> seen_embedders_;

  std::vector<std::unique_ptr<GlicCuiTracker>> cui_trackers_;

  // Number of user prompts submitted while the instance is in side panel mode.
  // Incremented on user input when current_ui_mode_ is kSidePanel, and logged
  // when the side panel is closed or tab is switched for the first time.
  size_t side_panel_prompt_count_ = 0;

  // Timestamp of the last tooltip impression, used to check whether the panel
  // closes within 10s of a tooltip showing.
  base::TimeTicks last_tooltip_impression_time_;

  // Tracks if a tooltip was shown during the current session.
  bool tooltip_shown_in_session_ = false;

  // Tracks whether a multi-tab tooltip is currently displayed to the user.
  bool tooltip_currently_visible_ = false;

  // Start time of the in-flight context upload, if any.
  std::optional<base::TimeTicks> last_upload_start_time_;

  base::WeakPtrFactory<GlicInstanceMetrics> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_
