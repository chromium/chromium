// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_

#include <optional>

#include "base/containers/enum_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/startup/startup_launch_infobar_manager.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "components/prefs/pref_member.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserProcess;

// IMPORTANT: If you add a new value to this enum, you MUST ensure the
// corresponding client calls `UpdateLaunchOnStartup()` from all possible code
// paths during browser startup.
//
// The StartupLaunchManager waits for ALL clients to "check in" before
// finalizing the startup configuration. If a reason is added here but never
// registers/unregisters, the manager will hang and never write to the registry.
//
// Reasons why Chrome should be launched on startup.
enum class StartupLaunchReason {
  kExtensions = 0,
  kGlic = 1,
  kForeground = 2,

  // Update these when adding/removing values.
  kMinValue = kExtensions,
  kMaxValue = kForeground,
};

// StartupLaunchManager registers with the OS so that Chrome launches on device
// startup - depending on the reasons why Chrome should launch on startup.
class StartupLaunchManager : public StartupLaunchInfoBarManager::Observer {
 public:
  // Clients should instantiate and own this class, and use
  // `UpdateLaunchOnStartup` to interact with StartupLaunchManager.
  class Client {
   public:
    explicit Client(StartupLaunchReason reason);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Registers/unregisters with the StartupLaunchManager.
    void SetLaunchOnStartup(bool enable_launch);

   private:
    const StartupLaunchReason launch_reason_;

    // Stores whether launch on startup is enabled for the client.
    // Null value implies client is not initialized yet.
    std::optional<bool> launch_enabled_ = std::nullopt;
  };

  explicit StartupLaunchManager(BrowserProcess* browser_process);
  ~StartupLaunchManager() override;

  DECLARE_USER_DATA(StartupLaunchManager);

  static StartupLaunchManager* From(BrowserProcess* browser_process);

  // StartupLaunchInfoBarManager::Observer:
  void OnInfoBarDismissed() override;

  // Releases the lock held by this instance. Once all active locks are
  // released or 1 minute has passed, a registry commit will occur.
  void CommitLaunchOnStartupState();

  // Sets the infobar manager. This is injected at runtime rather than in the
  // constructor to resolve circular dependencies between StartupLaunchManager
  // and the UI components that implement the infobar manager.
  // StartupLaunchManager owns the manager. If a manager already exists and
  // an infobar is being shown, it will be closed before the new manager
  // takes over.
  void SetInfoBarManager(std::unique_ptr<StartupLaunchInfoBarManager> manager);

  // Triggers the display of infobars across all eligible browser windows if
  // the preconditions (experiment state, not declined too many times) are met.
  // Requires a manager to be set via `SetInfoBarManager` beforehand.
  void MaybeShowInfoBars();

 private:
  // Methods to unregister/register individual reasons with the launch manager.
  void RegisterLaunchOnStartup(StartupLaunchReason reason);
  void UnregisterLaunchOnStartup(StartupLaunchReason reason);

  // Shared write locks will be held during startup to prevent writing to
  // registry until the all clients are ready. This prevents multiple, often
  // redundant registry writes during startup.
  void AcquireSharedWriteLock();
  void ReleaseSharedWriteLock();

  // Releases all locks if any client is still holding onto one, and flushing to
  // registry.
  void ForceReleaseAllLocks();

  virtual void UpdateLaunchOnStartup(
      std::optional<auto_launch_util::StartupLaunchMode> startup_launch_mode);

  // Returns `std::nullopt` if startup launch should be disabled.
  std::optional<auto_launch_util::StartupLaunchMode> GetStartupLaunchMode()
      const;

  // Updates the launch mode whenever the foreground launch pref is updated, eg.
  // through settings toggle.
  void OnLaunchOnStartupPrefChanged();

  // Helper method to apply the current state of the foreground launch pref.
  void UpdateForegroundLaunchRegistration();

  bool is_showing_infobar_ = false;

  // Task runner for making startup/login configuration changes that may
  // require file system or registry access.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Tracks number of clients holding shared write locks.
  size_t lock_counter_ = 0;

  // Tracks active launch reasons.
  base::EnumSet<StartupLaunchReason,
                StartupLaunchReason::kMinValue,
                StartupLaunchReason::kMaxValue>
      registered_launch_reasons_;

  // Stores the callback to trigger `ForceReleaseAllLocks` when initializing.
  base::OneShotTimer fallback_timer_;

  PrefMember<bool> foreground_launch_on_login_;

  std::optional<StartupLaunchInfoBarManager::InfoBarType> infobar_type_ =
      std::nullopt;

  std::unique_ptr<StartupLaunchInfoBarManager> infobar_manager_;

  base::ScopedObservation<StartupLaunchInfoBarManager,
                          StartupLaunchInfoBarManager::Observer>
      infobar_manager_observation_{this};

  ui::ScopedUnownedUserData<StartupLaunchManager> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_
