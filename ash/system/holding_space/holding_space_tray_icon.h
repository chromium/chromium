// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/scoped_observer.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class HoldingSpaceTrayIconPreview;
class Shelf;

// The icon used to represent holding space in its tray in the shelf.
class ASH_EXPORT HoldingSpaceTrayIcon : public views::View,
                                        public ShellObserver {
 public:
  METADATA_HEADER(HoldingSpaceTrayIcon);

  explicit HoldingSpaceTrayIcon(Shelf* shelf);
  HoldingSpaceTrayIcon(const HoldingSpaceTrayIcon&) = delete;
  HoldingSpaceTrayIcon& operator=(const HoldingSpaceTrayIcon&) = delete;
  ~HoldingSpaceTrayIcon() override;

  // Invoked when the system locale has changed.
  void OnLocaleChanged();

  // Updates the icon to match the current model state.
  void Init();

  // Clears the icon.
  void Reset();

  // Returns the shelf associated with this holding space tray icon.
  Shelf* shelf() { return shelf_; }

  // Called from HoldingSpaceTray when holding space model changes:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model);
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model);
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item);
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item);
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item);

 private:
  // views::View:
  base::string16 GetTooltipText(const gfx::Point& point) const override;
  int GetHeightForWidth(int width) const override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  void InitLayout();
  void UpdatePreferredSize();

  // Invoked when the specified preview has completed animating out. At this
  // point it is owned by `removed_previews_` and should be destroyed.
  void OnHoldingSpaceTrayIconPreviewAnimatedOut(HoldingSpaceTrayIconPreview*);

  // The shelf associated with this holding space tray icon.
  Shelf* const shelf_;

  // A preview is added to the tray icon to visually represent each holding
  // space item. Upon creation, previews are added to `previews_` where they
  // are owned until being animated out. Upon being animated out, previews are
  // moved to `removed_previews_` where they are owned until the out animation
  // completes and they are subsequently destroyed.
  std::vector<std::unique_ptr<HoldingSpaceTrayIconPreview>> previews_;
  std::vector<std::unique_ptr<HoldingSpaceTrayIconPreview>> removed_previews_;

  ScopedObserver<Shell,
                 ShellObserver,
                 &Shell::AddShellObserver,
                 &Shell::RemoveShellObserver>
      shell_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
