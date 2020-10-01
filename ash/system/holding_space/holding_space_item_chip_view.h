// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceItemViewDelegate;
class RoundedImageView;

// A button with an image derived from a file's thumbnail and file's name as the
// label.
class ASH_EXPORT HoldingSpaceItemChipView : public HoldingSpaceItemView {
 public:
  METADATA_HEADER(HoldingSpaceItemChipView);

  HoldingSpaceItemChipView(HoldingSpaceItemViewDelegate* delegate,
                           const HoldingSpaceItem* item);
  HoldingSpaceItemChipView(const HoldingSpaceItemChipView&) = delete;
  HoldingSpaceItemChipView& operator=(const HoldingSpaceItemChipView&) = delete;
  ~HoldingSpaceItemChipView() override;

 private:
  void UpdateImage();

  RoundedImageView* image_ = nullptr;
  views::Label* label_ = nullptr;

  std::unique_ptr<HoldingSpaceImage::Subscription> image_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
