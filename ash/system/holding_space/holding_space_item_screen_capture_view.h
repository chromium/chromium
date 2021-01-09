// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREEN_CAPTURE_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREEN_CAPTURE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceItemViewDelegate;
class RoundedImageView;

class ASH_EXPORT HoldingSpaceItemScreenCaptureView
    : public HoldingSpaceItemView {
 public:
  METADATA_HEADER(HoldingSpaceItemScreenCaptureView);

  HoldingSpaceItemScreenCaptureView(HoldingSpaceItemViewDelegate* delegate,
                                    const HoldingSpaceItem* item);
  HoldingSpaceItemScreenCaptureView(const HoldingSpaceItemScreenCaptureView&) =
      delete;
  HoldingSpaceItemScreenCaptureView& operator=(
      const HoldingSpaceItemScreenCaptureView&) = delete;
  ~HoldingSpaceItemScreenCaptureView() override;

 private:
  // HoldingSpaceItemView:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  base::string16 GetTooltipText(const gfx::Point& point) const override;
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) override;

  void UpdateImage();

  // Overlays a play icon over `image_`.
  void AddPlayIcon();

  RoundedImageView* image_ = nullptr;

  base::CallbackListSubscription image_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREEN_CAPTURE_VIEW_H_
