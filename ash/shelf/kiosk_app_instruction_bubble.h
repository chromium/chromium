// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_KIOSK_APP_INSTRUCTION_BUBBLE_H_
#define ASH_SHELF_KIOSK_APP_INSTRUCTION_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_bubble.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace views {
class Label;
class View;
}  // namespace views

namespace ash {

// The implementation of kiosk app instruction bubbles for the shelf.
class ASH_EXPORT KioskAppInstructionBubble : public ShelfBubble {
 public:
  KioskAppInstructionBubble(views::View* anchor,
                            ShelfAlignment alignment,
                            SkColor background_color);

  KioskAppInstructionBubble(const KioskAppInstructionBubble&) = delete;
  KioskAppInstructionBubble& operator=(const KioskAppInstructionBubble&) =
      delete;
  ~KioskAppInstructionBubble() override;

  // views::View:
  void OnThemeChanged() override;

  void Show();
  void Hide();

 protected:
  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

 private:
  // BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  views::Label* title_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_KIOSK_APP_INSTRUCTION_BUBBLE_H_