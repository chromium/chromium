// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of IME feature pod button.
class ASH_EXPORT IMEFeaturePodController : public FeaturePodControllerBase,
                                           public IMEObserver {
 public:
  explicit IMEFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  IMEFeaturePodController(const IMEFeaturePodController&) = delete;
  IMEFeaturePodController& operator=(const IMEFeaturePodController&) = delete;

  ~IMEFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  void Update();

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_active) override;

  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_
