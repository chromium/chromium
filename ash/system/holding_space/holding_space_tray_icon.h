// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class HoldingSpaceTrayIconPreview;
class Shelf;
class Shell;

// The icon used to represent holding space in its tray in the shelf.
class ASH_EXPORT HoldingSpaceTrayIcon : public views::View,
                                        public ShellObserver,
                                        public ShelfConfig::Observer {
  METADATA_HEADER(HoldingSpaceTrayIcon, views::View)

 public:
  explicit HoldingSpaceTrayIcon(Shelf* shelf);
  HoldingSpaceTrayIcon(const HoldingSpaceTrayIcon&) = delete;
  HoldingSpaceTrayIcon& operator=(const HoldingSpaceTrayIcon&) = delete;
  ~HoldingSpaceTrayIcon() override;

  // Updates whether or not this holding space icon is in drop target state.
  // If `did_drop_to_pin` is true, the user has just performed a drag-and-drop
  // to pin action. Otherwise a drag may still be in progress or the user action
  // did not result in an item being pinned to holding space.
  void UpdateDropTargetState(bool is_drop_target, bool did_drop_to_pin);

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

  // Called from HoldingSpaceTray when holding space model changes:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model);
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model);
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item);
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item);
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item);

  // Sets if updates should be animated.
  void set_should_animate_updates(bool should_animate_updates) {
    should_animate_updates_ = should_animate_updates;
  }

 private:
  class ResizeAnimation;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  // ShellObserver:
  void OnShellDestroying() override;
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // ShelfConfigObserver:
  void OnShelfConfigUpdated() override;

  void InitLayout();

  // Invoked when the specified preview has completed animating out. At this
  // point it is owned by `removed_previews_` and should be destroyed.
  void OnOldItemAnimatedOut(HoldingSpaceTrayIconPreview*,
                            const base::RepeatingClosure& callback);

  // Called when all obsolete previews have been removed during previews update.
  void OnOldItemsRemoved();

  // Defines parameters for how to animate a given `preview`.
  struct PreviewAnimationParams {
    raw_ptr<HoldingSpaceTrayIconPreview> preview;
    base::TimeDelta delay;
  };

  // Calculates parameters for how to animate shift/in existing/new items.
  std::vector<PreviewAnimationParams> CalculateAnimateShiftParams();
  std::vector<PreviewAnimationParams> CalculateAnimateInParams();

  // Ensures that preview layers are stacked to match ordering in `item_ids_`.
  void EnsurePreviewLayerStackingOrder();

  // The shelf associated with this holding space tray icon.
  const raw_ptr<Shelf> shelf_;

  // Whether or not this holding space tray icon is currently in drop target
  // state. When in drop target state, preview indices are offset from their
  // standard positions by a fixed amount.
  bool is_drop_target_ = false;

  // True if updates should be animated, false otherwise. Generally speaking,
  // updates are animated only if they occur mid-session. Updates that occur
  // during session start/unlock or on profile change should not be animated.
  bool should_animate_updates_ = false;

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
  raw_ptr<views::View> previews_container_ = nullptr;

  // Helper to run icon resize animation.
  std::unique_ptr<ResizeAnimation> resize_animation_;

  base::ScopedObservation<Shell, ShellObserver> shell_observer_{this};

  base::ScopedObservation<ShelfConfig, ShelfConfig::Observer>
      shelf_config_observer_{this};

  // The factory to which callbacks for stages of the previews list update are
  // bound to. The goal is to easily cancel in-progress updates if the list of
  // items gets updated.
  base::WeakPtrFactory<HoldingSpaceTrayIcon> previews_update_weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
