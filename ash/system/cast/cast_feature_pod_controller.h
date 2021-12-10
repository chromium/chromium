// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_CAST_CAST_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of cast feature pod button.
class ASH_EXPORT CastFeaturePodController
    : public FeaturePodControllerBase,
      public CastConfigController::Observer {
 public:
  explicit CastFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  CastFeaturePodController(const CastFeaturePodController&) = delete;
  CastFeaturePodController& operator=(const CastFeaturePodController&) = delete;

  ~CastFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

  // CastConfigControllerObserver:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

 private:
  void Update();

  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_FEATURE_POD_CONTROLLER_H_
