// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_IMPL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_IMPL_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_page_handler.h"

class Profile;

namespace borealis {

// Helper class responsible for launching borealis' apps.
class BorealisAppLauncherImpl : public BorealisAppLauncher {
 public:
  using OnLaunchedCallback = base::OnceCallback<void(LaunchResult)>;

  explicit BorealisAppLauncherImpl(Profile* profile);
  ~BorealisAppLauncherImpl() override;

  // Launch the given |app_id|'s associated application. This can be the
  // borealis launcher itself or one of its GuestOsRegistry apps.
  void Launch(std::string app_id,
              BorealisLaunchSource source,
              OnLaunchedCallback callback) override;

  // Launch the given |app_id|'s associated application with the given |args|.
  // This can be the borealis launcher itself or one of its GuestOsRegistry
  // apps. |source| indicates the source of the launch request.
  void Launch(std::string app_id,
              const std::vector<std::string>& args,
              BorealisLaunchSource source,
              OnLaunchedCallback callback) override;

 private:
  // Launch the given |app_id|'s associated application with the given |args|.
  // This can be the borealis launcher itself or one of its GuestOsRegistry
  // apps. |source| indicates the source of the launch request.
  // |motd_user_action| is used to identify action on the motd dialog that
  // was taken by the user.
  void LaunchAfterMOTD(std::string app_id,
                       const std::vector<std::string>& args,
                       BorealisLaunchSource source,
                       OnLaunchedCallback callback,
                       UserMotdAction motd_user_action);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  base::WeakPtrFactory<BorealisAppLauncherImpl> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_LAUNCHER_IMPL_H_
