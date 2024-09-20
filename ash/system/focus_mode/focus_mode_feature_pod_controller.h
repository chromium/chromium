// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of the feature pod button that allows users to toggle whether
// Focus Mode is enabled or disabled, and that allows users to navigate to a
// more detailed page with the Focus Mode settings.
class ASH_EXPORT FocusModeFeaturePodController
    : public FeaturePodControllerBase,
      public FocusModeController::Observer {
 public:
  explicit FocusModeFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  FocusModeFeaturePodController(const FocusModeFeaturePodController&) = delete;
  FocusModeFeaturePodController& operator=(
      const FocusModeFeaturePodController&) = delete;
  ~FocusModeFeaturePodController() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;

  // FocusModeController::Observer:
  void OnFocusModeChanged(FocusModeSession::State session_state) override;
  void OnTimerTick(const FocusModeSession::Snapshot& session_snapshot) override;
  void OnInactiveSessionDurationChanged(
      const base::TimeDelta& session_duration) override;
  void OnActiveSessionDurationChanged(
      const FocusModeSession::Snapshot& session_snapshot) override;

 private:
  void UpdateUI(const FocusModeSession::Snapshot& session_snapshot);

  // Owned by views hierarchy.
  raw_ptr<FeatureTile, DanglingUntriaged> tile_ = nullptr;

  const raw_ptr<UnifiedSystemTrayController, DanglingUntriaged>
      tray_controller_;

  base::WeakPtrFactory<FocusModeFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_FEATURE_POD_CONTROLLER_H_
