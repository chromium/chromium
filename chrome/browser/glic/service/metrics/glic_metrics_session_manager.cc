// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_metrics_session_manager.h"

#include <locale>

#include "base/containers/enum_set.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/service/glic_state_tracker.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/common/chrome_features.h"

namespace glic {

// Holds the state and timers for a single, active session.
// Its lifetime defines the duration of the session.
class ActiveSession {
 public:
  friend class GlicMetricsSessionManager;
  explicit ActiveSession(GlicMetricsSessionManager* owner,
                         bool initial_is_active,
                         base::TimeTicks start_time,
                         int initial_pinned_tab_count)
      : owner_(owner),
        activity_tracker_(initial_is_active,
                          "Glic.Instance.Session.UninterruptedActiveDuration"),
        // Sessions are only started when the instance is visible.
        visibility_tracker_(
            true,
            "Glic.Instance.Session.UninterruptedVisibleDuration"),
        start_time_(start_time),
        pinned_tab_count_(initial_pinned_tab_count),
        session_max_pinned_tab_count_(initial_pinned_tab_count) {
    // base::Unretained is safe here because the timer is owned by this class,
    // so the callback will not be invoked after destruction.
    start_timer_.Start(
        FROM_HERE, features::kGlicMetricsSessionStartTimeout.Get(),
        base::BindOnce(&ActiveSession::Start, base::Unretained(this)));
  }
  ~ActiveSession() {}

  ActiveSession(const ActiveSession&) = delete;
  ActiveSession& operator=(const ActiveSession&) = delete;

  // Event handlers
  void OnVisibilityChanged(bool is_visible) {
    HandleStateChange(is_visible, &visibility_tracker_,
                      &visibility_debounce_timer_, &hidden_timer_,
                      features::kGlicMetricsSessionHiddenTimeout.Get(),
                      GlicMultiInstanceSessionEndReason::kHidden,
                      base::BindOnce(&ActiveSession::OnVisibilityDebounceFired,
                                     base::Unretained(this)));
  }

  void OnActivationChanged(bool is_active) {
    HandleStateChange(is_active, &activity_tracker_,
                      &activation_debounce_timer_, &inactivity_timer_,
                      features::kGlicMetricsSessionInactivityTimeout.Get(),
                      GlicMultiInstanceSessionEndReason::kInactivity,
                      base::BindOnce(&ActiveSession::OnActivationDebounceFired,
                                     base::Unretained(this)));
  }

  void OnUserInputSubmitted(mojom::WebClientMode mode) {
    inputs_modes_used_.Put(mode);
    if (pinned_tab_count_ > 1) {
      input_submitted_with_multiple_pinned_tabs_ = true;
    }
  }

  void SetPinnedTabCount(int tab_count) {
    pinned_tab_count_ = tab_count;
    session_max_pinned_tab_count_ =
        std::max(session_max_pinned_tab_count_, tab_count);
  }

  void Start() {
    if (is_started()) {
      return;
    }
    state_ = State::kStarted;
    // Reset start timer in case this is called by onUserInputSubmitted.
    start_timer_.Stop();
    base::RecordAction(base::UserMetricsAction("Glic.Instance.Session.Start"));

    owner_->NotifySessionStarted();

    // If the session starts while the instance is already inactive (but
    // visible), begin the inactivity timeout immediately.
    if (!activity_tracker_.state()) {
      // base::Unretained is safe because the timer is a member of
      // `ActiveSession` which is owned by `owner_`. The timer will be
      // destroyed along with `ActiveSession` before the owner is destroyed.
      inactivity_timer_.Start(
          FROM_HERE, features::kGlicMetricsSessionInactivityTimeout.Get(),
          base::BindOnce(&GlicMetricsSessionManager::FinishSession,
                         base::Unretained(owner_),
                         GlicMultiInstanceSessionEndReason::kInactivity));
    }
  }

  void OnEvent(GlicInstanceEvent event) {
    if (!is_started()) {
      // Do not log events until session has started.
      return;
    }
    if (event_counts_[event] == 0) {
      base::UmaHistogramEnumeration("Glic.Instance.Session.HadEvent", event);
    }
    event_counts_[event]++;
  }

  bool is_started() const { return state_ == State::kStarted; }

  // Getters for metric recording
  int GetEventCount(GlicInstanceEvent event) {
    const auto it = event_counts_.find(event);
    return it == event_counts_.end() ? 0 : it->second;
  }
  base::TimeTicks start_time() const { return start_time_; }
  const base::EnumSet<mojom::WebClientMode,
                      mojom::WebClientMode::kMinValue,
                      mojom::WebClientMode::kMaxValue>&
  inputs_modes_used() const {
    return inputs_modes_used_;
  }

  int session_max_pinned_tab_count() const {
    return session_max_pinned_tab_count_;
  }

  bool input_submitted_with_multiple_pinned_tabs() const {
    return input_submitted_with_multiple_pinned_tabs_;
  }

 private:
  // Called when the visibility debounce timer fires. This means the instance
  // has been visible long enough to consider it truly visible again , so
  // we stop the hidden timer.
  void OnVisibilityDebounceFired() { hidden_timer_.Stop(); }
  // Called when the activation debounce timer fires. This means the instance
  // has been active long enough to consider it truly active again, so we
  // stop the inactivity timer.
  void OnActivationDebounceFired() { inactivity_timer_.Stop(); }

  // Helper to handle state transitions (active/inactive, visible/hidden).
  // Manages debounce timers to ignore brief state flickers and end
  // timers to finish the session after prolonged inactive/hidden states.
  void HandleStateChange(bool new_state,
                         GlicStateTracker* tracker,
                         base::OneShotTimer* debounce_timer,
                         base::OneShotTimer* end_timer,
                         base::TimeDelta end_timeout,
                         GlicMultiInstanceSessionEndReason end_reason,
                         base::OnceClosure debounce_callback) {
    // If the session hasn't started yet (is pending), a transition to
    // 'false' (hidden or inactive) immediately cancels it.
    if (state_ == State::kPending) {
      if (!new_state) {
        owner_->FinishSession(end_reason);
        return;
      }
      tracker->OnStateChanged(new_state);
      return;
    }

    tracker->OnStateChanged(new_state);

    if (new_state) {
      // Transitioned to 'true' (active/visible).
      // If the 'end' timer was running (meaning we were previously
      // 'false'), start a debounce timer. If we stay 'true' long enough,
      // the debounce callback will fire and stop the 'end' timer.
      if (end_timer->IsRunning()) {
        debounce_timer->Start(
            FROM_HERE, features::kGlicMetricsSessionRestartDebounceTimer.Get(),
            std::move(debounce_callback));
      }
    } else {
      // Transitioned to 'false' (inactive/hidden).
      // If a debounce timer was running, it means we briefly flickered to
      // 'true'. Stop the debounce timer and ignore this flicker (the
      // original 'end' timer continues running).
      if (debounce_timer->IsRunning()) {
        debounce_timer->Stop();
        return;
      }

      // Otherwise, this is a genuine transition to 'false'. Start the 'end'
      // timer to finish the session if we remain in this state too long.
      end_timer->Start(FROM_HERE, end_timeout,
                       base::BindOnce(&GlicMetricsSessionManager::FinishSession,
                                      base::Unretained(owner_), end_reason));
    }
  }

  enum class State {
    kPending,
    kStarted,
  };

  const raw_ptr<GlicMetricsSessionManager> owner_;
  State state_ = State::kPending;

  GlicStateTracker activity_tracker_;
  GlicStateTracker visibility_tracker_;

  base::OneShotTimer start_timer_;
  base::OneShotTimer hidden_timer_;
  base::OneShotTimer inactivity_timer_;
  base::OneShotTimer visibility_debounce_timer_;
  base::OneShotTimer activation_debounce_timer_;

  base::TimeTicks start_time_;
  base::EnumSet<mojom::WebClientMode,
                mojom::WebClientMode::kMinValue,
                mojom::WebClientMode::kMaxValue>
      inputs_modes_used_;
  base::flat_map<GlicInstanceEvent, int> event_counts_;
  int pinned_tab_count_ = 0;
  int session_max_pinned_tab_count_ = 0;
  bool input_submitted_with_multiple_pinned_tabs_ = false;
};

// GlicMetricsSessionManager implementation
GlicMetricsSessionManager::GlicMetricsSessionManager(GlicInstanceMetrics* owner)
    : owner_(owner) {}

GlicMetricsSessionManager::~GlicMetricsSessionManager() = default;

void GlicMetricsSessionManager::OnVisibilityChanged(bool is_visible) {
  if (is_visible && !active_session_) {
    // If the instance becomes visible and there is no active session, create
    // a new pending session.
    CreatePendingSession();
  }
  if (active_session_) {
    active_session_->OnVisibilityChanged(is_visible);
  }
}

void GlicMetricsSessionManager::OnActivationChanged(bool is_active) {
  if (is_active && !active_session_) {
    // This is to catch the case when an instance is left visible but inactive
    // and its session times out. When the instance is reactivated it will not
    // get a visibility update and so the session is started here.
    CreatePendingSession();
  }

  if (active_session_) {
    active_session_->OnActivationChanged(is_active);
  }
}

void GlicMetricsSessionManager::OnUserInputSubmitted(
    mojom::WebClientMode mode) {
  if (active_session_) {
    active_session_->Start();
    active_session_->OnUserInputSubmitted(mode);
  }
}

void GlicMetricsSessionManager::OnOwnerDestroyed() {
  FinishSession(GlicMultiInstanceSessionEndReason::kOwnerDestroyed);
}

void GlicMetricsSessionManager::NotifySessionStarted() {
  owner_->OnSessionStarted();
}

void GlicMetricsSessionManager::OnEvent(GlicInstanceEvent event) {
  if (active_session_) {
    active_session_->OnEvent(event);
  }
}

void GlicMetricsSessionManager::SetPinnedTabCount(int tab_count) {
  if (active_session_) {
    active_session_->SetPinnedTabCount(tab_count);
  }
}

int GlicMetricsSessionManager::GetEventCount(GlicInstanceEvent event) {
  if (active_session_) {
    return active_session_->GetEventCount(event);
  }
  return 0;
}

void GlicMetricsSessionManager::FinishSession(
    GlicMultiInstanceSessionEndReason reason) {
  if (!active_session_) {
    return;
  }

  if (active_session_->is_started()) {
    base::RecordAction(base::UserMetricsAction("Glic.Instance.Session.End"));
    base::UmaHistogramEnumeration("Glic.Instance.Session.EndReason", reason);
    active_session_->activity_tracker_.Finalize();
    active_session_->visibility_tracker_.Finalize();

    const base::TimeDelta session_duration =
        base::TimeTicks::Now() - active_session_->start_time();
    const base::TimeDelta inactive_time =
        session_duration - active_session_->activity_tracker_.total_duration();
    const base::TimeDelta hidden_time =
        session_duration -
        active_session_->visibility_tracker_.total_duration();

    base::UmaHistogramCustomTimes(
        "Glic.Instance.Session.TotalActiveDuration",
        active_session_->activity_tracker_.total_duration(),
        base::Milliseconds(1), base::Hours(24), 50);
    base::UmaHistogramCustomTimes("Glic.Instance.Session.TotalInactiveDuration",
                                  inactive_time, base::Milliseconds(1),
                                  base::Hours(24), 50);
    base::UmaHistogramCustomTimes(
        "Glic.Instance.Session.TotalVisibleDuration",
        active_session_->visibility_tracker_.total_duration(),
        base::Milliseconds(1), base::Hours(24), 50);
    base::UmaHistogramCustomTimes("Glic.Instance.Session.TotalHiddenDuration",
                                  hidden_time, base::Milliseconds(1),
                                  base::Hours(24), 50);

    base::UmaHistogramCustomTimes("Glic.Instance.Session.Duration",
                                  session_duration, base::Milliseconds(1),
                                  base::Hours(1), 50);
    base::UmaHistogramCounts100(
        "Glic.Instance.Session.TurnCount",
        GetEventCount(GlicInstanceEvent::kTurnCompleted));

    base::UmaHistogramCounts100("Glic.Instance.Session.CreateTabCount",
                                GetEventCount(GlicInstanceEvent::kCreateTab));
    base::UmaHistogramCounts100("Glic.Instance.Session.ToggleCount",
                                GetEventCount(GlicInstanceEvent::kToggle));

    InputModesUsed modes_used = InputModesUsed::kNone;
    bool has_audio =
        active_session_->inputs_modes_used().Has(mojom::WebClientMode::kAudio);
    bool has_text =
        active_session_->inputs_modes_used().Has(mojom::WebClientMode::kText);
    if (has_audio) {
      modes_used =
          has_text ? InputModesUsed::kTextAndAudio : InputModesUsed::kOnlyAudio;
    } else if (has_text) {
      modes_used = InputModesUsed::kOnlyText;
    }
    base::UmaHistogramEnumeration("Glic.Instance.Session.InputModesUsed",
                                  modes_used);

    base::UmaHistogramBoolean(
        "Glic.Instance.Session.MultipleTabsPinnedInAnyTurn",
        active_session_->input_submitted_with_multiple_pinned_tabs());
    base::UmaHistogramCounts100(
        "Glic.Instance.Session.MaxPinnedTabs",
        active_session_->session_max_pinned_tab_count());

    owner_->OnSessionFinished();
  }

  // Resetting is now just destroying the ActiveSession object.
  active_session_.reset();
}

void GlicMetricsSessionManager::CreatePendingSession() {
  CHECK(!active_session_);
  active_session_ = std::make_unique<ActiveSession>(
      this, owner_->is_active(), base::TimeTicks::Now(),
      owner_->GetPinnedTabCount());
}

}  // namespace glic
