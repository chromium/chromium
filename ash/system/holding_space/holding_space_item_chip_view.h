// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/system/holding_space/holding_space_animation_registry.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class ImageButton;
class Label;
}  // namespace views

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceViewDelegate;
class ProgressIndicator;
class RoundedImageView;

// A button with an image derived from a file's thumbnail and file's name as the
// label.
class ASH_EXPORT HoldingSpaceItemChipView : public HoldingSpaceItemView {
  METADATA_HEADER(HoldingSpaceItemChipView, HoldingSpaceItemView)

 public:
  HoldingSpaceItemChipView(HoldingSpaceViewDelegate* delegate,
                           const HoldingSpaceItem* item);
  HoldingSpaceItemChipView(const HoldingSpaceItemChipView&) = delete;
  HoldingSpaceItemChipView& operator=(const HoldingSpaceItemChipView&) = delete;
  ~HoldingSpaceItemChipView() override;

 private:
  // HoldingSpaceItemView:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override;
  void OnPrimaryActionVisibilityChanged(bool visible) override;
  void OnSelectionUiChanged() override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnThemeChanged() override;

  // Invoked during `label`'s paint sequence to paint its optional mask. Note
  // that `label` is only masked when the `primary_action_container()` is
  // visible to avoid overlapping.
  void OnPaintLabelMask(views::Label* label, gfx::Canvas* canvas);

  // Invoked when the secondary action is pressed. This will be one of either
  // `secondary_action_pause_` or `secondary_action_resume_`.
  void OnSecondaryActionPressed();

  // Posts a task to invoke `UpdateTooltipText()` iff one is not already posted.
  void ScheduleUpdateTooltipText();

  void UpdateImage();
  void UpdateImageAndProgressIndicatorVisibility();
  void UpdateImageTransform();
  void UpdateLabels();
  void UpdateSecondaryAction();
  void UpdateTooltipText();

  // Owned by view hierarchy.
  raw_ptr<RoundedImageView> image_ = nullptr;
  raw_ptr<views::Label> primary_label_ = nullptr;
  raw_ptr<views::Label> secondary_label_ = nullptr;
  raw_ptr<views::View> secondary_action_container_ = nullptr;
  raw_ptr<views::ImageButton> secondary_action_pause_ = nullptr;
  raw_ptr<views::ImageButton> secondary_action_resume_ = nullptr;
  raw_ptr<ProgressIndicator> progress_indicator_ = nullptr;

  base::CallbackListSubscription image_skia_changed_subscription_;
  base::CallbackListSubscription progress_ring_animation_changed_subscription_;
  std::vector<base::CallbackListSubscription>
      tooltip_text_dependency_changed_subscriptions_;

  // Used to post a task to `UpdateTooltipText()` iff one is not already posted.
  base::OneShotTimer update_tooltip_text_scheduler_;

  base::WeakPtrFactory<HoldingSpaceItemChipView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   HoldingSpaceItemChipView,
                   HoldingSpaceItemView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::HoldingSpaceItemChipView)

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
