// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

enum class ExitType {
  // Chrome was shutdown cleanly.
  kClean,

  // Shutdown was forced by the OS. On some OS's (such as Windows) this causes
  // chrome to shutdown differently.
  kForcedShutdown,

  kCrashed,
};

class CrashedLock;

// ExitTypeService tracks the exit status of the last and current time Chrome
// was run. In the case of a crash, the last exit type may persist across
// multiple restarts. Consider the following sequence:
// 1. Chrome crashes
// 2. User opens chrome and does not close session crashed bubble.
// 3. User restarts chrome.
// At this point, the last exit type is still `kCrashed`. This is done to ensure
// the user has the opportunity to restore after a crash and does not lose data.
// Also note that there are a number of subtle cases that trigger 2. For
// example, chrome is launched in the background, or chrome is launched in
// incognito mode.
//
// The following actions are treated as acknowledging the crash:
// . A new browser is created outside of startup flow. While the crash bubble
//   may still be visible, assume that if the user created the browser the user
//   no longer cares about the previous session data.
// . No more locks are outstanding (corresponds to no crash bubbles).
class ExitTypeService : public KeyedService {
 public:
  // Used to lock the last exit status to crashed. The existence of a lock does
  // not necessarily guarantee the status remains crashed.
  class CrashedLock {
   public:
    CrashedLock(const CrashedLock&) = delete;
    CrashedLock& operator=(const CrashedLock&) = delete;
    ~CrashedLock();

   private:
    friend class ExitTypeService;

    explicit CrashedLock(ExitTypeService* service);

    raw_ptr<ExitTypeService> service_;
  };

  explicit ExitTypeService(Profile* profile);
  ExitTypeService(const ExitTypeService&) = delete;
  ExitTypeService& operator=(const ExitTypeService&) = delete;
  ~ExitTypeService() override;

  // Returns the ExitTypeService for the specified profile. This returns
  // null for non-regular profiles.
  static ExitTypeService* GetInstanceForProfile(Profile* profile);

  // Returns the last session exit type. If supplied an incognito profile,
  // this will return the value for the original profile. See class description
  // for caveats around `kCrashed`.
  static ExitType GetLastSessionExitType(Profile* profile);

  // Sets the ExitType for the profile. This may be invoked multiple times
  // during shutdown; only the first such change (the transition from
  // ExitType::kCrashed to one of the other values) is written to prefs, any
  // later calls are ignored.
  //
  // NOTE: this is invoked internally on a normal shutdown, but is public so
  // that it can be invoked when the user logs out/powers down (WM_ENDSESSION),
  // or to handle backgrounding/foregrounding on mobile.
  void SetCurrentSessionExitType(ExitType exit_type);

  // See class description for caveats around `kCrashed`.
  ExitType last_session_exit_type() const { return last_session_exit_type_; }

  std::unique_ptr<CrashedLock> CreateCrashedLock();

  Profile* profile() { return profile_; }

  // Adds a callback that is called once the crash state is acknowledged and
  // will be reset if chrome shutdowns cleanly. If not waiting for the user to
  // acknowledge the crash, `callback` is dropped and is never called.
  void AddCrashAckCallback(base::OnceClosure callback);

  bool waiting_for_user_to_ack_crash() const {
    return waiting_for_user_to_ack_crash_;
  }

  void SetLastSessionExitTypeForTest(ExitType type) {
    last_session_exit_type_ = type;
  }

  void SetWaitingForUserToAckCrashForTest(bool value) {
    waiting_for_user_to_ack_crash_ = value;
  }

 private:
  friend class ExitTypeServiceFactory;
  class BrowserTabObserverImpl;

  // Checks if the user acknowledged the crash.
  void CheckUserAckedCrash();

  // Called from the ~CrashedLock.
  void CrashedLockDestroyed();

  // Called when a tab is created from a non-startup Browser.
  void OnTabAddedToNonStartupBrowser();

  // Returns true if currently waiting for the user to acknowledge the crash,
  // and the user did in fact acknowledge the crash.
  bool DidUserAckCrash() const;

  bool IsWaitingForRestore() const {
    return static_cast<bool>(restore_subscription_);
  }

  bool IsWaitingForBrowser() const {
    return browser_tab_observer_.get() != nullptr;
  }

  // Called once session restore completes.
  void OnSessionRestoreDone(Profile* profile, int tabs_restored);

  const raw_ptr<Profile> profile_;
  ExitType last_session_exit_type_;
  ExitType current_session_exit_type_;

  // Indicates waiting for the user to acknowledge the crash. This is set from
  // the constructor if `last_session_exit_type_` is `kCrashed` and to false
  // once the user acknowledges the crash.
  bool waiting_for_user_to_ack_crash_;

  // Set to true once a restore completes.
  bool did_restore_complete_ = false;

  // Used to detect when a new browser is added.
  std::unique_ptr<BrowserTabObserverImpl> browser_tab_observer_;

  // Number of outstanding CrashedLock created.
  int crashed_lock_count_ = 0;

  // Used for observing a restore.
  base::CallbackListSubscription restore_subscription_;

  // Temporary place that stores the current session exit type if
  // SetCurrentSessionExitType() is called while waiting for the user to
  // acknowledge the crash.
  std::optional<ExitType> exit_type_to_apply_on_ack_;

  // Callbacks run once the user acknowledges the crash.
  std::vector<base::OnceClosure> crash_ack_callbacks_;
};

#endif  // CHROME_BROWSER_SESSIONS_EXIT_TYPE_SERVICE_H_
