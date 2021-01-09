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

  // Updates the list of previews shown in the icon. The icon will be changed to
  // show previews for holding space items in `items`. The order of previews in
  // the icon will match the order in `items`.
  // The items are updated in the following stages:
  // 1. Remove obsolete items.
  // 2. Update the icon preferred size.
  // 3. Shift existing items to their new position.
  // 4. Animate new items in.
  void UpdatePreviews(const std::vector<const HoldingSpaceItem*> items);

  // Clears the icon.
  void Clear();

  // Returns the shelf associated with this holding space tray icon.
  Shelf* shelf() { return shelf_; }

  // Called from HoldingSpaceTray when holding space model changes:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model);
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model);
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item);
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item);
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item);

 private:
  class ResizeAnimation;

  // views::View:
  int GetHeightForWidth(int width) const override;
  gfx::Size CalculatePreferredSize() const override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  void InitLayout();

  // Invoked when the specified preview has completed animating out. At this
  // point it is owned by `removed_previews_` and should be destroyed.
  void OnOldItemAnimatedOut(HoldingSpaceTrayIconPreview*,
                            const base::RepeatingClosure& callback);

  // Called when all obsolete previews have been animated out during previews
  // update.
  void OnOldItemsRemoved();

  // Starts shift animation for existing items. Done while updating the previews
  // shown in the icon.
  void ShiftExistingItems();

  // Animates new items in. Done while updating the previews shown in the icon.
  void AnimateInNewItems();

  // The shelf associated with this holding space tray icon.
  Shelf* const shelf_;

  // A preview is added to the tray icon to visually represent each holding
  // space item. Upon creation, previews are added to `previews_by_id_` where
  // they are owned until being animated out. Upon being animated out, previews
  // are moved to `removed_previews_` where they are owned until the out
  // animation completes and they are subsequently destroyed. NOTE:
  // `previews_by_id_` maps holding space item IDs to their respective previews.
  std::map<std::string, std::unique_ptr<HoldingSpaceTrayIconPreview>>
      previews_by_id_;
  std::vector<std::unique_ptr<HoldingSpaceTrayIconPreview>> removed_previews_;

  // Ordered list of holding space item IDs represented in the tray icon
  // (including items that are not currently visible).
  std::vector<std::string> item_ids_;

  // A view that serves as a parent for previews' layers. Used to easily
  // translate all the previews within the icon during resize animation.
  views::View* previews_container_ = nullptr;

  // Helper to run icon resize animation.
  std::unique_ptr<ResizeAnimation> resize_animation_;

  ScopedObserver<Shell,
                 ShellObserver,
                 &Shell::AddShellObserver,
                 &Shell::RemoveShellObserver>
      shell_observer_{this};

  // The factory to which callbacks for stages of the previews list update are
  // bound to. The goal is to easily cancel in-progress updates if the list of
  // items gets updated.
  base::WeakPtrFactory<HoldingSpaceTrayIcon> previews_update_weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
