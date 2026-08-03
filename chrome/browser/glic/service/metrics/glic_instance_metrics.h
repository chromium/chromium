// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance_metrics_backwards_compatibility.h"
#include "chrome/browser/glic/public/glic_window_invocation_tracker.h"
#include "chrome/browser/glic/service/glic_state_tracker.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"

class PrefService;
class Profile;

namespace metrics {

class ProfileMetricsService;
}

namespace content {
class WebContents;
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

using SafeEmbedderKey =
    std::variant<tabs::TabHandle, FloatingEmbedderKey, TabEmbedderKey>;

// Tracks and logs lifecycle events for a single GlicInstance.
class GlicInstanceMetrics : public GlicInstanceMetricsBackwardsCompatibility {
 public:
  explicit GlicInstanceMetrics(
      const metrics::ProfileMetricsService* profile_metrics_service,
      Profile* profile = nullptr);
  GlicInstanceMetrics(
      const metrics::ProfileMetricsService* profile_metrics_service,
      GlicSharingManagerInternal* sharing_manager,
      enterprise_reporting::SaasUsageReportingController*
          saas_usage_reporting_controller,
      Profile* profile = nullptr);
  ~GlicInstanceMetrics() override;

  GlicInstanceMetrics(const GlicInstanceMetrics&) = delete;
  GlicInstanceMetrics& operator=(const GlicInstanceMetrics&) = delete;

  // `GlicInstanceMetricsBackwardsCompatibility`:
  void OnUserInputSubmitted(mojom::WebClientMode mode) override;
  void DidRequestContextFromTab(tabs::TabInterface& tab) override;
  void OnResponseStarted() override;
  void OnResponseStopped(mojom::ResponseStopCause cause) override;
  void OnTurnCompleted(mojom::WebClientModel model, base::TimeDelta duration);
  void OnReaction(mojom::MetricUserInputReactionType reaction_type);
  void OnGlicScrollAttempt();
  void OnGlicScrollComplete(bool success);

  // Called when the opt-in CTA is shown.
  void OnOptinImpression();

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

  // Called when this instance is shown in an inactive side panel.
  void OnShowInactiveSidePanel(mojom::InvocationSource invocation_source);

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

  // Called when Close is called on the instance.
  void OnClose();

  // Called when Toggle is called on the instance.
  void OnToggle(glic::mojom::InvocationSource source,
                const ShowOptions& options,
                bool is_showing,
                std::unique_ptr<GlicWindowInvocationTracker>
                    invocation_tracker = nullptr);

  // Called when the UI is shown and it was not already showing for this
  // instance.
  void OnOpen(glic::mojom::InvocationSource source, const ShowOptions& options);

  bool MarkShownAndCheckIfFirstTime(EmbedderKey key);

  void ResetShownState(EmbedderKey key);

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

  // Called when GlicInstanceImpl::UninterruptActorTask is called.
  void UninterruptActorTask();

  // Called when GlicInstanceImpl::InterruptActorTask is called.
  void InterruptActorTask();

  // Called when GlicInstanceImpl::WebUiStateChanged is called.
  void OnWebUiStateChanged(mojom::WebUiState state);

  // Called when the client is ready to show.
  void OnClientReady(EmbedderType type);

  void OnUserResizeStarted(const gfx::Size& start_size);
  void OnUserResizeEnded(const gfx::Size& end_size);

  void OnZoomLevelChange();

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

  std::optional<mojom::InvocationSource> initial_invocation_source() const {
    return initial_invocation_source_;
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
    bool did_request_context_ = false;
    EmbedderType ui_mode_ = EmbedderType::kUnknown;
    mojom::WebClientMode input_mode_ = mojom::WebClientMode::kUnknown;
    bool pending_scroll_complete_ = false;
    ukm::SourceId chosen_source_id_ = ukm::NoURLSourceId();
  };

  // Logs the given event to the EventTotals histogram, and if the count is 0,
  // also logs to the EventCounts histogram. Increments the counter.
  void LogEvent(GlicInstanceEvent event);
  int GetEventCount(GlicInstanceEvent event);

  // Called by the session manager when it starts and ends.
  void OnSessionStarted();
  void OnSessionFinished();

  void OnPinnedTabsChanged(const std::vector<tabs::TabInterface*>& pinned_tabs);

  // Records the response latency (from user input submitted to response stop)
  // by the number of attached tabs.
  void RecordResponseLatencyByAttachedTabCount(base::TimeDelta latency);

  void RecordSkillsInvokeFunnelStep(SkillsInvokeFunnel invoke_funnel);
  void RecordAndResetAutoOpenPdfMetric();
  void MaybeRecordOptInImpression();

  // Records the duration and prompt count for the first time the side panel is
  // closed or the tab is switched.
  void MaybeRecordFirstSidePanelOpenMetrics(base::TimeDelta duration);

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
  base::EnumSet<mojom::WebClientMode> inputs_modes_used_;

  // The last web ui state received.
  mojom::WebUiState last_web_ui_state_ = mojom::WebUiState::kUninitialized;
  // The last invocation source that was used to show the panel.
  mojom::InvocationSource last_invocation_source_ =
      mojom::InvocationSource::kUnsupported;
  std::optional<mojom::InvocationSource> initial_invocation_source_ =
      std::nullopt;
  bool did_open_ = false;
  // Timestamp of last show start.
  base::TimeTicks invocation_start_time_;
  base::TimeTicks web_ui_load_start_time_;

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

  GlicMetricsSessionManager session_manager_;
  base::TimeTicks last_session_end_time_;
  int session_count_ = 0;
  int pinned_tab_count_ = 0;

  std::map<tabs::TabHandle, int> tab_depths_;

  bool is_client_ready_ = false;
  bool is_opt_in_pending_ = false;
  bool has_consented_ = false;

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
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_METRICS_H_
