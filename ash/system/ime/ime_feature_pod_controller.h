// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/macros.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of IME feature pod button.
class ASH_EXPORT IMEFeaturePodController : public FeaturePodControllerBase,
                                           public IMEObserver {
 public:
  IMEFeaturePodController(UnifiedSystemTrayController* tray_controller);
  ~IMEFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  void Update();

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_active) override;

  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
  FeaturePodButton* button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(IMEFeaturePodController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_
