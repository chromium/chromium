// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeaturePodButton;
class FeatureTile;

// Controller of a feature pod button for toggling the built-in privacy screen.
class ASH_EXPORT PrivacyScreenFeaturePodController
    : public FeaturePodControllerBase,
      public PrivacyScreenController::Observer {
 public:
  PrivacyScreenFeaturePodController();
  ~PrivacyScreenFeaturePodController() override;

  PrivacyScreenFeaturePodController(const PrivacyScreenFeaturePodController&) =
      delete;
  PrivacyScreenFeaturePodController& operator=(
      const PrivacyScreenFeaturePodController&) = delete;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  void TogglePrivacyScreen();
  void UpdateButton();
  void UpdateTile();

  // PrivacyScreenController::Observer:
  void OnPrivacyScreenSettingChanged(bool enabled, bool notify_ui) override;

  // Owned by the views hierarchy.
  raw_ptr<FeaturePodButton, DanglingUntriaged | ExperimentalAsh> button_ =
      nullptr;
  raw_ptr<FeatureTile, DanglingUntriaged | ExperimentalAsh> tile_ = nullptr;

  base::WeakPtrFactory<PrivacyScreenFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_FEATURE_POD_CONTROLLER_H_
