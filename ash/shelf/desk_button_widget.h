// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DESK_BUTTON_WIDGET_H_
#define ASH_SHELF_DESK_BUTTON_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_component.h"
#include "ash/style/pill_button.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
class Shelf;

// The desk button provides an overview of existing desks and quick access to
// them. The button is only visible in clamshell mode and disappears when in
// overview.
class ASH_EXPORT DeskButtonWidget : public ShelfComponent,
                                    public views::Widget {
 public:
  explicit DeskButtonWidget(Shelf* shelf);

  DeskButtonWidget(const DeskButtonWidget&) = delete;
  DeskButtonWidget& operator=(const DeskButtonWidget&) = delete;

  ~DeskButtonWidget() override;

  // Calculate the width in horizontal alignment based on the screen size, and
  // the height in vertical alignment.
  int GetPreferredLength() const;

  // Whether the desk button should currently be visible.
  bool ShouldBeVisible() const;

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

  // Called when shelf layout manager detects a locale change.
  void HandleLocaleChange();

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container);

 private:
  class DelegateView;
  DelegateView* delegate_view_ = nullptr;

  gfx::Rect target_bounds_;

  Shelf* const shelf_;
};

}  // namespace ash

#endif  // ASH_SHELF_DESK_BUTTON_WIDGET_H_
