// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SESSION_LENGTH_LIMIT_MODEL_H_
#define ASH_SYSTEM_MODEL_SESSION_LENGTH_LIMIT_MODEL_H_

#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

// Model to manage coutdown timer when the session length is limited.
class SessionLengthLimitModel : public SessionObserver {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when |remaining_session_time| or |limit_state| is updated.
    virtual void OnSessionLengthLimitUpdated() = 0;
  };

  // LIMIT_NONE: The session does not have length limit.
  // LIMIT_SET: The session has length limit.
  // LIMIT_EXPIRING_SOON: The session will expire soon.
  enum LimitState { LIMIT_NONE, LIMIT_SET, LIMIT_EXPIRING_SOON };

  SessionLengthLimitModel();

  SessionLengthLimitModel(const SessionLengthLimitModel&) = delete;
  SessionLengthLimitModel& operator=(const SessionLengthLimitModel&) = delete;

  ~SessionLengthLimitModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnSessionLengthLimitChanged() override;

  base::TimeDelta remaining_session_time() const {
    return remaining_session_time_;
  }
  LimitState limit_state() const { return limit_state_; }

 private:
  // Recalculate |limit_state_| and |remaining_session_time_|.
  void Update();

  base::TimeDelta remaining_session_time_;
  LimitState limit_state_ = LIMIT_NONE;

  std::unique_ptr<base::RepeatingTimer> timer_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SESSION_LENGTH_LIMIT_MODEL_H_
