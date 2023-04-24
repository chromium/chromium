// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DRIVE_PINNING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DRIVE_PINNING_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {
class DrivePinningScreenView;

// Controller for the Drive Pinning Screen.
class DrivePinningScreen : public BaseScreen,
                           drivefs::pinning::PinManager::Observer {
 public:
  using TView = DrivePinningScreenView;

  enum class Result { ACCEPT, DECLINE, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  DrivePinningScreen(base::WeakPtr<DrivePinningScreenView> view,
                     const ScreenExitCallback& exit_callback);

  DrivePinningScreen(const DrivePinningScreen&) = delete;
  DrivePinningScreen& operator=(const DrivePinningScreen&) = delete;

  ~DrivePinningScreen() override;

  void CalculateRequiredSpace();

 private:
  // BaseScreen:
  bool ShouldBeSkipped(const WizardContext& context) const override;
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // drivefs::pinning::PinManager::Observer
  void OnProgress(const drivefs::pinning::Progress& progress) override;

  // Called when the user turn on drive pinning on the screen.
  void OnAccept();

  // Called when the user decline drive pinning on the screen.
  void OnDecline();

  base::WeakPtr<DrivePinningScreenView> view_;
  ScreenExitCallback exit_callback_;
  bool drive_pinning_available_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DRIVE_PINNING_SCREEN_H_
