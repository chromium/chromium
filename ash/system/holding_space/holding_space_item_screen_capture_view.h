// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREEN_CAPTURE_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREEN_CAPTURE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceViewDelegate;
class RoundedImageView;

class ASH_EXPORT HoldingSpaceItemScreenCaptureView
    : public HoldingSpaceItemView {
 public:
  METADATA_HEADER(HoldingSpaceItemScreenCaptureView);

  HoldingSpaceItemScreenCaptureView(HoldingSpaceViewDelegate* delegate,
                                    const HoldingSpaceItem* item);
  HoldingSpaceItemScreenCaptureView(const HoldingSpaceItemScreenCaptureView&) =
      delete;
  HoldingSpaceItemScreenCaptureView& operator=(
      const HoldingSpaceItemScreenCaptureView&) = delete;
  ~HoldingSpaceItemScreenCaptureView() override;

 private:
  // HoldingSpaceItemView:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& point) const override;
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                 uint32_t updated_fields) override;
  void OnThemeChanged() override;

  void UpdateImage();

  // Owned by view hierarchy.
  views::View* border_ = nullptr;
  RoundedImageView* image_ = nullptr;
  views::ImageView* play_icon_ = nullptr;

  base::CallbackListSubscription image_skia_changed_subscription_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   HoldingSpaceItemScreenCaptureView,
                   HoldingSpaceItemView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::HoldingSpaceItemScreenCaptureView)

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREEN_CAPTURE_VIEW_H_
