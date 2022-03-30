// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_QUICK_PAIR_COMPANION_APP_INSTALLER_H_
#define CHROME_BROWSER_ASH_QUICK_PAIR_COMPANION_APP_INSTALLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace ash {
namespace quick_pair {

// CompanionAppInstaller takes the companion app name and figures out
// whether or not the companion app found is installable
// Call CheckAppState to use
class CompanionAppInstaller {
 public:
  enum class CompanionAppState {
    kNotAvailable = 0,
    kAvailableToDownload = 1,
    kInstalled = 2,
  };

  CompanionAppInstaller();
  CompanionAppInstaller(const CompanionAppInstaller&) = delete;
  CompanionAppInstaller& operator=(const CompanionAppInstaller&) = delete;
  ~CompanionAppInstaller();

  void CheckAppState(
      const std::string& package_name,
      base::OnceCallback<void(CompanionAppInstaller::CompanionAppState)>
          on_companion_app_state_checked);

 private:
  void OnCheckAppIsInstallable(
      base::OnceCallback<void(CompanionAppInstaller::CompanionAppState)>
          callback,
      bool is_installable);

  base::WeakPtrFactory<CompanionAppInstaller> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_QUICK_PAIR_COMPANION_APP_INSTALLER_H_
