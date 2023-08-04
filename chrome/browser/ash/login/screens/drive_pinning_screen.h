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
#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

class Profile;

namespace ash {
class DrivePinningScreenView;

// Controller for the Drive Pinning Screen.
class DrivePinningScreen : public BaseScreen,
                           drivefs::pinning::PinManager::Observer {
 public:
  using TView = DrivePinningScreenView;

  enum class Result { NEXT, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  // Apply the deferred perf `kOobeDrivePinningEnabledDeferred`.
  static void ApplyDrivePinningPref(Profile* profile);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  DrivePinningScreen(base::WeakPtr<DrivePinningScreenView> view,
                     const ScreenExitCallback& exit_callback);

  DrivePinningScreen(const DrivePinningScreen&) = delete;
  DrivePinningScreen& operator=(const DrivePinningScreen&) = delete;

  ~DrivePinningScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  // Returns true if the bulk-pinning manager is in the right stage and started
  // computing the required space.
  [[nodiscard]] bool CalculateRequiredSpace();

  std::string RetrieveChoobeSubtitle();

  void OnProgressForTest(const drivefs::pinning::Progress& progress);

 private:
  // BaseScreen:
  bool ShouldBeSkipped(const WizardContext& context) const override;
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  ScreenSummary GetScreenSummary() override;

  // drivefs::pinning::PinManager::Observer
  void OnProgress(const drivefs::pinning::Progress& progress) override;

  void OnNext(bool drive_pinning);

  drivefs::pinning::Stage drive_pinning_stage_ =
      drivefs::pinning::Stage::kStopped;

  base::WeakPtr<DrivePinningScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DRIVE_PINNING_SCREEN_H_
