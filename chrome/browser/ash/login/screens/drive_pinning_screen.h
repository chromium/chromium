// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DRIVE_PINNING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DRIVE_PINNING_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"

class Profile;

namespace ash {
class DrivePinningScreenView;

// Controller for the Drive Pinning Screen.
class DrivePinningScreen
    : public BaseScreen,
      drive::DriveIntegrationService::Observer,
      public screens_common::mojom::DrivePinningPageHandler,
      public OobeMojoBinder<screens_common::mojom::DrivePinningPageHandler,
                            screens_common::mojom::DrivePinningPage> {
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

  void set_ignore_choobe_controller_state_for_testing(
      bool ignore_chobe_controller) {
    ignore_choobe_controller_state_for_tests_ = ignore_chobe_controller;
  }

  // Starts calculating the required space. This should only be called once, in
  // the event DriveFS restarts the `DrivePinningScreen` will handle restarting
  // calculation.
  void StartCalculatingRequiredSpace();

  std::string RetrieveChoobeSubtitle();

  void OnProgressForTest(const drivefs::pinning::Progress& progress);

 private:
  void CalculateRequiredSpace();

  // BaseScreen:
  bool ShouldBeSkipped(const WizardContext& context) const override;
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  ScreenSummary GetScreenSummary() override;

  // DriveIntegrationService::Observer implementation.
  void OnBulkPinProgress(const drivefs::pinning::Progress& progress) override;
  void OnBulkPinInitialized() override;

  void OnNext(bool drive_pinning);
  void SetRequiredSpaceInfo(std::u16string required_space,
                            std::u16string free_space);

  // screens_common::mojom::DrivePinningPageHandler
  void OnReturnClicked(bool enable_drive_pinning) override;
  void OnNextClicked(bool enable_drive_pinning) override;

  drivefs::pinning::Stage drive_pinning_stage_ =
      drivefs::pinning::Stage::kStopped;
  bool started_calculating_space_ = false;

  bool ignore_choobe_controller_state_for_tests_ = false;

  // The number of times bulk pinning is initialized.
  int bulk_pinning_initializations_ = 0;

  base::WeakPtr<DrivePinningScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DRIVE_PINNING_SCREEN_H_
