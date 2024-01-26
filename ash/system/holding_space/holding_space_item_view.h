// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class ToggleImageButton;
}  // namespace views

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceViewDelegate;

// Base class for `HoldingSpaceItemChipView` and
// `HoldingSpaceItemScreenCaptureView`. Note that `HoldingSpaceItemView` may
// temporarily outlive its associated `HoldingSpaceItem` when it is being
// animated out.
class ASH_EXPORT HoldingSpaceItemView : public views::View,
                                        public HoldingSpaceModelObserver {
  METADATA_HEADER(HoldingSpaceItemView, views::View)

 public:
  HoldingSpaceItemView(HoldingSpaceViewDelegate*, const HoldingSpaceItem*);
  HoldingSpaceItemView(const HoldingSpaceItemView&) = delete;
  HoldingSpaceItemView& operator=(const HoldingSpaceItemView&) = delete;
  ~HoldingSpaceItemView() override;

  // Returns `view` cast as a `HoldingSpaceItemView`. Note that this performs a
  // DCHECK to assert that `view` is in fact a `HoldingSpaceItemView` instance.
  static HoldingSpaceItemView* Cast(views::View* view);
  static const HoldingSpaceItemView* Cast(const views::View* view);

  // Returns if `view` is an instance of `HoldingSpaceItemView`.
  static bool IsInstance(const views::View* view);

  // Resets the view. Called when the tray bubble starts closing to ensure
  // that any references that may be outlived are cleared out.
  void Reset();

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override;

  // Starts a drag from this view at the location specified by the given `event`
  // and with the specified `source`. Note that this method copies the logic of
  // `views::View::DoDrag()` as a workaround to that API being private.
  void StartDrag(const ui::LocatedEvent& event,
                 ui::mojom::DragEventSource source);

  const HoldingSpaceItem* item() const { return item_; }
  const std::string& item_id() const { return item_id_; }

  void SetSelected(bool selected);
  bool selected() const { return selected_; }

 protected:
  views::Builder<views::ImageView> CreateCheckmarkBuilder();
  views::Builder<views::View> CreatePrimaryActionBuilder(
      bool apply_accent_colors = false,
      const gfx::Size& min_size = gfx::Size());

  virtual void OnPrimaryActionVisibilityChanged(bool visible) {}
  virtual void OnSelectionUiChanged();

  HoldingSpaceViewDelegate* delegate() { return delegate_; }
  views::ImageView* checkmark() { return checkmark_; }
  views::View* primary_action_container() { return primary_action_container_; }

 private:
  void OnPaintFocus(gfx::Canvas* canvas, gfx::Size size);
  void OnPaintSelect(gfx::Canvas* canvas, gfx::Size size);
  void OnPrimaryActionPressed();
  void UpdatePrimaryAction();

  // NOTE: This view may outlive `delegate_` and/or `item_` during destruction
  // since the widget is closed asynchronously and the model is updated prior
  // to animation completion.
  raw_ptr<HoldingSpaceViewDelegate> delegate_ = nullptr;
  raw_ptr<const HoldingSpaceItem> item_ = nullptr;

  // Cache the id of the associated holding space item so that it can be
  // accessed even after `item_` has been destroyed. Note that `item_` may be
  // destroyed if this view is in the process of animating out.
  const std::string item_id_;

  // Owned by view hierarchy.
  raw_ptr<views::ImageView> checkmark_ = nullptr;
  raw_ptr<views::View> primary_action_container_ = nullptr;
  raw_ptr<views::ImageButton> primary_action_cancel_ = nullptr;
  raw_ptr<views::ToggleImageButton> primary_action_pin_ = nullptr;

  // Owners for the layers used to paint focused and selected states.
  std::unique_ptr<ui::LayerOwner> selected_layer_owner_;
  std::unique_ptr<ui::LayerOwner> focused_layer_owner_;

  // Whether or not this view is selected.
  bool selected_ = false;

  // Subscription to be notified of `item_` deletion.
  base::RepeatingClosureList::Subscription item_deletion_subscription_;

  // Subscription to be notified of changes to `delegate_''s selection UI.
  base::RepeatingClosureList::Subscription selection_ui_changed_subscription_;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer_{this};

  base::WeakPtrFactory<HoldingSpaceItemView> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, HoldingSpaceItemView, views::View)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::HoldingSpaceItemView)

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
