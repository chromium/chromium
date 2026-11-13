// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_END_CONDITION_DETECTOR_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_END_CONDITION_DETECTOR_H_

#include "base/functional/callback.h"
#include "base/power_monitor/power_observer.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#else
#include "base/callback_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace ttc {

// Detects conditions outside a session that should end it: the device is about
// to suspend, the screen locks, or (on ChromeOS) the active user switches. This
// keeps a session from holding the microphone open while the user is away.
class SessionEndConditionDetector
    : public base::PowerSuspendObserver
#if BUILDFLAG(IS_CHROMEOS)
    ,
      public user_manager::UserManager::UserSessionStateObserver,
      public session_manager::SessionManagerObserver
#endif  // BUILDFLAG(IS_CHROMEOS)
{
 public:
  enum class Reason {
    kSuspend,
    kScreenLock,
#if BUILDFLAG(IS_CHROMEOS)
    kUserSwitch,
#endif  // BUILDFLAG(IS_CHROMEOS)
  };

  explicit SessionEndConditionDetector(
      base::RepeatingCallback<void(Reason)> callback);
  SessionEndConditionDetector(const SessionEndConditionDetector&) = delete;
  SessionEndConditionDetector& operator=(const SessionEndConditionDetector&) =
      delete;
  ~SessionEndConditionDetector() override;

  // base::PowerSuspendObserver:
  void OnSuspend() override;

#if BUILDFLAG(IS_CHROMEOS)
  // user_manager::UserManager::UserSessionStateObserver:
  // TODO(crbug.com/278643115): Switch to
  // `session_manager::SessionManagerObserver::OnActiveSessionSwitched()`.
  void ActiveUserChanged(user_manager::User* active_user) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;
#else
  // Called by the screen lock subscription. Visible for testing:
  // `ui::AddScreenLockCallback()` has no fake.
  void OnScreenLockChanged(bool locked);
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  base::RepeatingCallback<void(Reason)> callback_;

#if BUILDFLAG(IS_CHROMEOS)
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      user_session_state_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
#else
  base::CallbackListSubscription screen_lock_subscription_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_END_CONDITION_DETECTOR_H_
