// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/scoped_observer.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class HoldingSpaceTrayIconItem;
class Shelf;

// TODO(crbug.com/1142572): Implement content forward icon w/ motion spec.
// The icon used to represent holding space in its tray in the shelf.
class ASH_EXPORT HoldingSpaceTrayIcon : public views::View,
                                        public HoldingSpaceControllerObserver,
                                        public HoldingSpaceModelObserver,
                                        public ShellObserver {
 public:
  METADATA_HEADER(HoldingSpaceTrayIcon);

  explicit HoldingSpaceTrayIcon(Shelf* shelf);
  HoldingSpaceTrayIcon(const HoldingSpaceTrayIcon&) = delete;
  HoldingSpaceTrayIcon& operator=(const HoldingSpaceTrayIcon&) = delete;
  ~HoldingSpaceTrayIcon() override;

  // Returns the preferred main axis margin for this view.
  int GetPreferredMainAxisMargin() const;

  // Invoked when the system locale has changed.
  void OnLocaleChanged();

  // Returns the shelf associated with this holding space tray icon.
  Shelf* shelf() { return shelf_; }

 private:
  // views::View:
  base::string16 GetTooltipText(const gfx::Point& point) const override;

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  void InitLayout();
  void UpdatePreferredSize();

  // Invoked when the specified icon item has completed animated out. At this
  // point it is owned by `removed_icon_items_` and should be destroyed.
  void OnHoldingSpaceTrayIconItemAnimatedOut(HoldingSpaceTrayIconItem*);

  // The shelf associated with this holding space tray icon.
  Shelf* const shelf_;

  // An icon item is added to the tray icon to visually represent each holding
  // space item. Upon creation, icon items are added to `icon_items_` where they
  // are owned until being animated out. Upon being animated out, icon items are
  // moved to `removed_icon_items_` where they are owned until the out animation
  // completes and are subsequently destroyed.
  std::vector<std::unique_ptr<HoldingSpaceTrayIconItem>> icon_items_;
  std::vector<std::unique_ptr<HoldingSpaceTrayIconItem>> removed_icon_items_;

  ScopedObserver<HoldingSpaceController, HoldingSpaceControllerObserver>
      controller_observer_{this};
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> model_observer_{
      this};
  ScopedObserver<Shell,
                 ShellObserver,
                 &Shell::AddShellObserver,
                 &Shell::RemoveShellObserver>
      shell_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
