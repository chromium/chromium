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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class FeaturePodButton;
class FeatureTile;

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
  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override;
  QsFeatureCatalogName GetCatalogName() override;
  void OnIconPressed() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShelfModelObserver:
  void ShelfPartyToggled(bool in_shelf_party) override;

 private:
  void Update();
  void UpdateButton();
  void UpdateTile();

  // Owned by the views hierarchy.
  raw_ptr<FeaturePodButton, ExperimentalAsh> button_ = nullptr;
  raw_ptr<FeatureTile, ExperimentalAsh> tile_ = nullptr;

  base::WeakPtrFactory<ShelfPartyFeaturePodController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_PARTY_FEATURE_POD_CONTROLLER_H_
