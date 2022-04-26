// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_KIOSK_APP_INSTRUCTION_BUBBLE_H_
#define ASH_SHELF_KIOSK_APP_INSTRUCTION_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_bubble.h"
#include "third_party/skia/include/core/SkColor.h"

namespace views {
class Label;
class View;
}  // namespace views

namespace ash {

// A shelf bubble instructing kiosk users to interact with the kiosk app menu
// button.
class ASH_EXPORT KioskAppInstructionBubble : public ShelfBubble {
 public:
  KioskAppInstructionBubble(views::View* anchor,
                            ShelfAlignment alignment,
                            SkColor background_color);

  KioskAppInstructionBubble(const KioskAppInstructionBubble&) = delete;
  KioskAppInstructionBubble& operator=(const KioskAppInstructionBubble&) =
      delete;
  ~KioskAppInstructionBubble() override;

 private:
  // views::View:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

  views::Label* title_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_KIOSK_APP_INSTRUCTION_BUBBLE_H_