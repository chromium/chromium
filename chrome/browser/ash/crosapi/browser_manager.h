// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace crosapi {

// Manages the lifetime of lacros-chrome, and its loading status. Observes the
// component updater for future updates. This class is a part of ash-chrome.
class BrowserManager : public session_manager::SessionManagerObserver {
 public:
  // Static getter of BrowserManager instance. In real use cases,
  // BrowserManager instance should be unique in the process.
  static BrowserManager* Get();

  BrowserManager();

  BrowserManager(const BrowserManager&) = delete;
  BrowserManager& operator=(const BrowserManager&) = delete;

  ~BrowserManager() override;

  // Initialize resources and start Lacros.
  //
  // NOTE: If InitializeAndStartIfNeeded finds Lacros disabled, it deletes the
  // user data directory.
  virtual void InitializeAndStartIfNeeded();

 protected:
  // NOTE: You may have to update tests if you make changes to State, as state_
  // is exposed via autotest_private.
  enum class State {
    // Lacros is not initialized yet.
    // Lacros-chrome loading depends on user type, so it needs to wait
    // for user session.
    NOT_INITIALIZED,

    // Lacros-chrome is unavailable. I.e., failed to load for some reason
    // or disabled.
    UNAVAILABLE,
  };
  // Changes |state| value.
  void SetState(State state);

 private:
  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Start a sequence to clear Lacros related data. It posts a task to remove
  // Lacros user data directory and if that is successful, calls
  // `OnLacrosUserDataDirRemoved()` to clear some prefs set by Lacros in Ash.
  // Call if Lacros is disabled and not running.
  void ClearLacrosData();

  // Called as a callback to `RemoveLacrosUserDataDir()`. `cleared` is set to
  // true if the directory existed and was removed successfully.
  void OnLacrosUserDataDirRemoved(bool cleared);

  // NOTE: The state is exposed to tests via autotest_private.
  State state_ = State::NOT_INITIALIZED;

  base::WeakPtrFactory<BrowserManager> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
