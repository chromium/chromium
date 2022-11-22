// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class KioskEnableScreenView;

// Representation independent class that controls screen for enabling
// consumer kiosk mode.
class KioskEnableScreen : public BaseScreen {
 public:
  KioskEnableScreen(base::WeakPtr<KioskEnableScreenView> view,
                    const base::RepeatingClosure& exit_callback);

  KioskEnableScreen(const KioskEnableScreen&) = delete;
  KioskEnableScreen& operator=(const KioskEnableScreen&) = delete;

  ~KioskEnableScreen() override;

 private:
  // BaseScreen implementation:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void HandleClose();
  void HandleEnable();

  // Callback for KioskAppManager::EnableConsumerModeKiosk().
  void OnEnableConsumerKioskAutoLaunch(bool success);

  // Callback for KioskAppManager::GetConsumerKioskModeStatus().
  void OnGetConsumerKioskAutoLaunchStatus(
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  base::WeakPtr<KioskEnableScreenView> view_;
  base::RepeatingClosure exit_callback_;

  // True if machine's consumer kiosk mode is in a configurable state.
  bool is_configurable_ = false;

  base::WeakPtrFactory<KioskEnableScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
