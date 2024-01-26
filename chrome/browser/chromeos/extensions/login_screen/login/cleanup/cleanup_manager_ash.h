// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_ASH_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_ASH_H_

#include <optional>

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager.h"

namespace chromeos {

// A singleton which manages the cleanup handlers in Ash.
class CleanupManagerAsh : public CleanupManager {
 public:
  static CleanupManagerAsh* Get();

  CleanupManagerAsh(const CleanupManagerAsh&) = delete;
  CleanupManagerAsh& operator=(const CleanupManagerAsh&) = delete;

 private:
  friend class base::NoDestructor<CleanupManagerAsh>;

  CleanupManagerAsh();
  ~CleanupManagerAsh() override;

  // CleanupManager:
  void InitializeCleanupHandlers() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_ASH_H_
