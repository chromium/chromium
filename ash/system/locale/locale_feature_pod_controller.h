// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of locale feature tile, which appears in demo mode and allows
// setting the language for demo mode content. To work on demo mode, see
// instructions at go/demo-mode-g3-cookbook. To force the feature tile to show
// in the emulator pass --qs-show-locale-tile.
class ASH_EXPORT LocaleFeaturePodController : public FeaturePodControllerBase {
 public:
  explicit LocaleFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  LocaleFeaturePodController(const LocaleFeaturePodController&) = delete;
  LocaleFeaturePodController& operator=(const LocaleFeaturePodController&) =
      delete;

  ~LocaleFeaturePodController() override;

  // FeaturePodControllerBase:
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  // Unowned.
  const raw_ptr<UnifiedSystemTrayController> tray_controller_;

  base::WeakPtrFactory<LocaleFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_
