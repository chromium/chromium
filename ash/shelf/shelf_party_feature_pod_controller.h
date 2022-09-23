// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_PARTY_FEATURE_POD_CONTROLLER_H_
#define ASH_SHELF_SHELF_PARTY_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

// Controller of a feature pod button that toggles shelf party mode.
class ASH_EXPORT ShelfPartyFeaturePodController
    : public FeaturePodControllerBase,
      public SessionObserver,
      public ShelfModelObserver {
 public:
  ShelfPartyFeaturePodController();
  ShelfPartyFeaturePodController(const ShelfPartyFeaturePodController&) =
      delete;
  ShelfPartyFeaturePodController& operator=(
      const ShelfPartyFeaturePodController&) = delete;
  ~ShelfPartyFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShelfModelObserver:
  void ShelfPartyToggled(bool in_shelf_party) override;

 private:
  void UpdateButton();

  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_PARTY_FEATURE_POD_CONTROLLER_H_
