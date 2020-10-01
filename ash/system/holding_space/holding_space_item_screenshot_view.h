// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREENSHOT_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREENSHOT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceItemViewDelegate;
class RoundedImageView;

class ASH_EXPORT HoldingSpaceItemScreenshotView : public HoldingSpaceItemView {
 public:
  METADATA_HEADER(HoldingSpaceItemScreenshotView);

  HoldingSpaceItemScreenshotView(HoldingSpaceItemViewDelegate* delegate,
                                 const HoldingSpaceItem* item);
  HoldingSpaceItemScreenshotView(const HoldingSpaceItemScreenshotView&) =
      delete;
  HoldingSpaceItemScreenshotView& operator=(
      const HoldingSpaceItemScreenshotView&) = delete;
  ~HoldingSpaceItemScreenshotView() override;

 private:
  void UpdateImage();

  RoundedImageView* image_ = nullptr;

  std::unique_ptr<HoldingSpaceImage::Subscription> image_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_SCREENSHOT_VIEW_H_
