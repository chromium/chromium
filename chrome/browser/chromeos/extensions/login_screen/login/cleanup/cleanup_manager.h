// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"

namespace chromeos {

// Common interface between `CleanupManagerAsh` and `CleanupManagerLacros`.
// Performs cleanup operations for the chrome.login.endSharedSession()
// extension API. A `CleanupManager` owns several `CleanupHandler`s. The
// individual handlers are in charge of cleaning up a specific
// component/service.
class CleanupManager {
 public:
  CleanupManager();

  CleanupManager(const CleanupManager&) = delete;
  CleanupManager& operator=(const CleanupManager&) = delete;

  virtual ~CleanupManager();

  using CleanupCallback =
      base::OnceCallback<void(const std::optional<std::string>& error)>;
  // Calls the cleanup handlers  and runs `callback` when the cleanup has
  // finished. After `Cleanup` is called and before `callback` is run,
  // `is_cleanup_in_progress()` returns true. Fails if there is another cleanup
  // already in progress.
  void Cleanup(CleanupCallback callback);

  bool is_cleanup_in_progress() const { return is_cleanup_in_progress_; }

  void SetCleanupHandlersForTesting(
      std::map<std::string, std::unique_ptr<CleanupHandler>> cleanup_handlers);

  void ResetCleanupHandlersForTesting();

  void SetIsCleanupInProgressForTesting(bool is_cleanup_in_progress);

 protected:
  // Overridden to initialize `CleanupHandler`s for Ash and Lacros.
  virtual void InitializeCleanupHandlers() = 0;

  void OnCleanupHandlerDone(base::RepeatingClosure barrier_closure,
                            const std::string& handler_name,
                            const std::optional<std::string>& error);

  void OnAllCleanupHandlersDone();

  void RecordHandlerMetrics(const std::string& handler_name,
                            const base::Time& start_time,
                            bool success);

  // Map of handler name to handler.
  std::map<std::string, std::unique_ptr<CleanupHandler>> cleanup_handlers_;
  std::vector<std::string> errors_;
  bool is_cleanup_in_progress_ = false;
  base::Time start_time_;
  CleanupCallback callback_;
  base::WeakPtrFactory<CleanupManager> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_H_
