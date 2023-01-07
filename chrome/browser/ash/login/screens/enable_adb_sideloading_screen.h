// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ENABLE_ADB_SIDELOADING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ENABLE_ADB_SIDELOADING_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

class PrefRegistrySimple;

namespace ash {

class EnableAdbSideloadingScreenView;

// Representation independent class that controls screen showing enable
// adb sideloading screen to users.
class EnableAdbSideloadingScreen : public BaseScreen {
 public:
  EnableAdbSideloadingScreen(base::WeakPtr<EnableAdbSideloadingScreenView> view,
                             const base::RepeatingClosure& exit_callback);

  EnableAdbSideloadingScreen(const EnableAdbSideloadingScreen&) = delete;
  EnableAdbSideloadingScreen& operator=(const EnableAdbSideloadingScreen&) =
      delete;

  ~EnableAdbSideloadingScreen() override;

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

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

  base::WeakPtr<EnableAdbSideloadingScreenView> view_;
  base::RepeatingClosure exit_callback_;
  base::WeakPtrFactory<EnableAdbSideloadingScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ENABLE_ADB_SIDELOADING_SCREEN_H_
