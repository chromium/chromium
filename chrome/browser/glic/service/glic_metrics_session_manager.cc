// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_metrics_session_manager.h"

#include <locale>

#include "base/containers/enum_set.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/service/glic_instance_metrics.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/common/chrome_features.h"

namespace glic {

// Holds the state and timers for a single, active session.
// Its lifetime defines the duration of the session.
class ActiveSession {
 public:
  friend class GlicMetricsSessionManager;
  explicit ActiveSession(GlicMetricsSessionManager* owner,
                         bool initial_is_active,
                         base::TimeTicks start_time)
      : owner_(owner), is_active_(initial_is_active), start_time_(start_time) {
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
    HandleStateChange(is_visible, is_visible_, visibility_debounce_timer_,
                      hidden_timer_,
                      features::kGlicMetricsSessionHiddenTimeout.Get(),
                      GlicMultiInstanceSessionEndReason::kHidden,
                      base::BindOnce(&ActiveSession::OnVisibilityDebounceFired,
                                     base::Unretained(this)));
  }

  void OnActivationChanged(bool is_active) {
    HandleStateChange(is_active, is_active_, activation_debounce_timer_,
                      inactivity_timer_,
                      features::kGlicMetricsSessionInactivityTimeout.Get(),
                      GlicMultiInstanceSessionEndReason::kInactivity,
                      base::BindOnce(&ActiveSession::OnActivationDebounceFired,
                                     base::Unretained(this)));
  }

  void OnUserInputSubmitted(mojom::WebClientMode mode) {
    inputs_modes_used_.Put(mode);
  }

  void OnTurnCompleted() { turn_count_++; }

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
    if (!is_active_) {
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

  bool is_started() const { return state_ == State::kStarted; }

  // Getters for metric recording
  base::TimeTicks start_time() const { return start_time_; }
  int turn_count() const { return turn_count_; }
  const base::EnumSet<mojom::WebClientMode,
                      mojom::WebClientMode::kMinValue,
                      mojom::WebClientMode::kMaxValue>&
  inputs_modes_used() const {
    return inputs_modes_used_;
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
                         bool& current_state,
                         base::OneShotTimer& debounce_timer,
                         base::OneShotTimer& end_timer,
                         base::TimeDelta end_timeout,
                         GlicMultiInstanceSessionEndReason end_reason,
                         base::OnceClosure debounce_callback) {
    if (new_state == current_state) {
      return;
    }

    // If the session hasn't started yet (is pending), a transition to
    // 'false' (hidden or inactive) immediately cancels it.
    if (state_ == State::kPending) {
      if (!new_state) {
        owner_->FinishSession(end_reason);
        return;
      }
      current_state = new_state;
      return;
    }

    current_state = new_state;

    if (new_state) {
      // Transitioned to 'true' (active/visible).
      // If the 'end' timer was running (meaning we were previously
      // 'false'), start a debounce timer. If we stay 'true' long enough,
      // the debounce callback will fire and stop the 'end' timer.
      if (end_timer.IsRunning()) {
        debounce_timer.Start(
            FROM_HERE, features::kGlicMetricsSessionRestartDebounceTimer.Get(),
            std::move(debounce_callback));
      }
    } else {
      // Transitioned to 'false' (inactive/hidden).
      // If a debounce timer was running, it means we briefly flickered to
      // 'true'. Stop the debounce timer and ignore this flicker (the
      // original 'end' timer continues running).
      if (debounce_timer.IsRunning()) {
        debounce_timer.Stop();
        return;
      }

      // Otherwise, this is a genuine transition to 'false'. Start the 'end'
      // timer to finish the session if we remain in this state too long.
      end_timer.Start(FROM_HERE, end_timeout,
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
  bool is_active_ = false;
  bool is_visible_ = false;

  base::OneShotTimer start_timer_;
  base::OneShotTimer hidden_timer_;
  base::OneShotTimer inactivity_timer_;
  base::OneShotTimer visibility_debounce_timer_;
  base::OneShotTimer activation_debounce_timer_;

  base::TimeTicks start_time_;
  int turn_count_ = 0;
  base::EnumSet<mojom::WebClientMode,
                mojom::WebClientMode::kMinValue,
                mojom::WebClientMode::kMaxValue>
      inputs_modes_used_;
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

void GlicMetricsSessionManager::OnTurnCompleted() {
  if (active_session_) {
    active_session_->OnTurnCompleted();
  }
}

void GlicMetricsSessionManager::OnOwnerDestroyed() {
  FinishSession(GlicMultiInstanceSessionEndReason::kOwnerDestroyed);
}

void GlicMetricsSessionManager::NotifySessionStarted() {
  owner_->OnSessionStarted();
}

void GlicMetricsSessionManager::FinishSession(
    GlicMultiInstanceSessionEndReason reason) {
  if (!active_session_) {
    return;
  }

  if (active_session_->is_started()) {
    base::RecordAction(base::UserMetricsAction("Glic.Instance.Session.End"));
    base::UmaHistogramEnumeration("Glic.Instance.Session.EndReason", reason);
    const base::TimeDelta session_duration =
        base::TimeTicks::Now() - active_session_->start_time();
    base::UmaHistogramCustomTimes("Glic.Instance.Session.Duration",
                                  session_duration, base::Milliseconds(1),
                                  base::Hours(1), 50);
    base::UmaHistogramCounts100("Glic.Instance.Session.TurnCount",
                                active_session_->turn_count());

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

    owner_->OnSessionFinished();
  }

  // Resetting is now just destroying the ActiveSession object.
  active_session_.reset();
}

void GlicMetricsSessionManager::CreatePendingSession() {
  CHECK(!active_session_);
  active_session_ = std::make_unique<ActiveSession>(this, owner_->is_active(),
                                                    base::TimeTicks::Now());
}

}  // namespace glic
