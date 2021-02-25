// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class KioskEnableScreenView;

// Representation independent class that controls screen for enabling
// consumer kiosk mode.
class KioskEnableScreen : public BaseScreen {
 public:
  KioskEnableScreen(KioskEnableScreenView* view,
                    const base::RepeatingClosure& exit_callback);
  ~KioskEnableScreen() override;

  // This method is called, when view is being destroyed. Note, if Screen
  // is destroyed earlier then it has to call SetScreen(nullptr).
  void OnViewDestroyed(KioskEnableScreenView* view);

 private:
  // BaseScreen implementation:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  void HandleClose();
  void HandleEnable();

  // Callback for KioskAppManager::EnableConsumerModeKiosk().
  void OnEnableConsumerKioskAutoLaunch(bool success);

  // Callback for KioskAppManager::GetConsumerKioskModeStatus().
  void OnGetConsumerKioskAutoLaunchStatus(
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  KioskEnableScreenView* view_;
  base::RepeatingClosure exit_callback_;

  // True if machine's consumer kiosk mode is in a configurable state.
  bool is_configurable_ = false;

  base::WeakPtrFactory<KioskEnableScreen> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(KioskEnableScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
