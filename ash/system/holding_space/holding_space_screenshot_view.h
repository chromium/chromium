// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_SCREENSHOT_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_SCREENSHOT_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class HoldingSpaceItem;

namespace tray {
class RoundedImageView;
}  // namespace tray

class ASH_EXPORT HoldingSpaceScreenshotView : public views::View {
 public:
  explicit HoldingSpaceScreenshotView(const HoldingSpaceItem* item);
  HoldingSpaceScreenshotView(const HoldingSpaceScreenshotView&) = delete;
  HoldingSpaceScreenshotView& operator=(const HoldingSpaceScreenshotView&) =
      delete;
  ~HoldingSpaceScreenshotView() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  void Update();

  const HoldingSpaceItem* const item_;
  tray::RoundedImageView* image_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_SCREENSHOT_VIEW_H_
