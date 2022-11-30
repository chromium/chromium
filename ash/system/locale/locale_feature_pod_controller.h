// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of locale feature pod button.
class ASH_EXPORT LocaleFeaturePodController : public FeaturePodControllerBase {
 public:
  explicit LocaleFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  LocaleFeaturePodController(const LocaleFeaturePodController&) = delete;
  LocaleFeaturePodController& operator=(const LocaleFeaturePodController&) =
      delete;

  ~LocaleFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

 private:
  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_
