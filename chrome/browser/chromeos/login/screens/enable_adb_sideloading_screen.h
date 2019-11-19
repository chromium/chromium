// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_ADB_SIDELOADING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_ADB_SIDELOADING_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

class PrefRegistrySimple;

namespace chromeos {

// Representation independent class that controls screen showing enable
// adb sideloading screen to users.
class EnableAdbSideloadingScreen : public BaseScreen {
 public:
  EnableAdbSideloadingScreen(EnableAdbSideloadingScreenView* view,
                             const base::RepeatingClosure& exit_callback);
  ~EnableAdbSideloadingScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  // Called by EnableAdbSideloadingHandler.
  void OnViewDestroyed(EnableAdbSideloadingScreenView* view);

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  base::RepeatingClosure* exit_callback() { return &exit_callback_; }

 private:
  void OnQueryAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled);
  void OnEnableAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code);

  void OnEnable();
  void OnCancel();
  void OnLearnMore();

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  EnableAdbSideloadingScreenView* view_;
  base::RepeatingClosure exit_callback_;
  base::WeakPtrFactory<EnableAdbSideloadingScreen> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EnableAdbSideloadingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_ADB_SIDELOADING_SCREEN_H_
