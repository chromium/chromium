// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// A singleton which manages the cleanup handlers.
class CleanupManager {
 public:
  static CleanupManager* Get();

  CleanupManager(const CleanupManager&) = delete;
  CleanupManager& operator=(const CleanupManager&) = delete;

  using CleanupCallback = base::OnceCallback<void(absl::optional<std::string>)>;
  // Calls the cleanup handlers  and runs `callback` when the cleanup has
  // finished. After `Cleanup` is called and before `callback` is run,
  // `is_cleanup_in_progress()` returns true. Fails if there is another cleanup
  // already in progress.
  void Cleanup(CleanupCallback callback);

  bool is_cleanup_in_progress() const { return is_cleanup_in_progress_; }

  void SetCleanupHandlersForTesting(
      std::vector<std::unique_ptr<CleanupHandler>> cleanup_handlers);

  void ResetCleanupHandlersForTesting();

  void SetIsCleanupInProgressForTesting(bool is_cleanup_in_progress);

 private:
  friend class base::NoDestructor<CleanupManager>;

  CleanupManager();
  ~CleanupManager();

  void InitializeCleanupHandlers();

  void OnCleanupHandlerDone(base::RepeatingClosure barrier_closure,
                            const absl::optional<std::string>& error);

  void OnAllCleanupHandlersDone();

  std::vector<std::unique_ptr<CleanupHandler>> cleanup_handlers_;
  std::vector<std::string> errors_;
  bool is_cleanup_in_progress_ = false;
  CleanupCallback callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_H_
