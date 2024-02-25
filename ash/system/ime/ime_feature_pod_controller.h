// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeatureTile;
class UnifiedSystemTrayController;

// Controller of IME feature tile.
class ASH_EXPORT IMEFeaturePodController : public FeaturePodControllerBase,
                                           public IMEObserver {
 public:
  explicit IMEFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  IMEFeaturePodController(const IMEFeaturePodController&) = delete;
  IMEFeaturePodController& operator=(const IMEFeaturePodController&) = delete;

  ~IMEFeaturePodController() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  void Update();

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_active) override;

  const raw_ptr<UnifiedSystemTrayController> tray_controller_;

  // Owned by the views hierarchy.
  raw_ptr<FeatureTile, DanglingUntriaged> tile_ = nullptr;

  base::WeakPtrFactory<IMEFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_IME_FEATURE_POD_CONTROLLER_H_
