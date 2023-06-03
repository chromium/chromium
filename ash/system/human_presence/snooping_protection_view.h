// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_VIEW_H_
#define ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/human_presence/snooping_protection_controller.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/scoped_observation.h"

namespace ash {

// The icon in the system tray notifying a user that a second person has been
// detected looking over their shoulder.
class ASH_EXPORT SnoopingProtectionView
    : public TrayItemView,
      public SnoopingProtectionController::Observer {
 public:
  explicit SnoopingProtectionView(Shelf* shelf);
  SnoopingProtectionView(const SnoopingProtectionView&) = delete;
  SnoopingProtectionView& operator=(const SnoopingProtectionView&) = delete;
  ~SnoopingProtectionView() override;

  // views::TrayItemView:
  const char* GetClassName() const override;
  void HandleLocaleChange() override;
  void UpdateLabelOrImageViewColor(bool active) override;

  // SnoopingProtectionController::Observer:
  void OnSnoopingStatusChanged(bool snooper) override;
  void OnSnoopingProtectionControllerDestroyed() override;

 private:
  base::ScopedObservation<SnoopingProtectionController,
                          SnoopingProtectionController::Observer>
      controller_observation_{this};

  // Must be last.
  base::WeakPtrFactory<SnoopingProtectionView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_VIEW_H_
