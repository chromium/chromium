// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DESK_BUTTON_WIDGET_H_
#define ASH_SHELF_DESK_BUTTON_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_component.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DeskButton;
class Shelf;
enum class ShelfAlignment;

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

  Shelf* shelf() const { return shelf_; }
  bool is_horizontal_shelf() const { return is_horizontal_shelf_; }
  bool is_expanded() const { return is_expanded_; }

  // Calculate the width in horizontal alignment based on the screen size, and
  // the height in vertical alignment.
  int GetPreferredLength() const;

  // Get the expanded width of the desk button based on whether the screen
  // width has passed a certain threshold.
  int GetPreferredExpandedWidth() const;

  // Calculates and returns bounds for the shrunken version of the button with
  // the current positioning.
  gfx::Rect GetTargetShrunkBounds() const;

  // Calculates and returns bounds for the expanded version of the button with
  // the current positioning.
  gfx::Rect GetTargetExpandedBounds() const;

  // Depending on what child view has focus, either focus out of the desk
  // button, or pass the focus to the next view. `reverse` indicates backward
  // focusing, otherwise forward focusing.
  void MaybeFocusOut(bool reverse);

  // Whether the desk button should currently be visible.
  bool ShouldBeVisible() const;

  // Sets whether the desk button is in expanded state and sets bounds
  // accordingly.
  void SetExpanded(bool expanded);

  // Updates expanded state and values impacted by shelf alignment change.
  void PrepareForAlignmentChange(ShelfAlignment new_alignment);

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

  // Called when shelf layout manager detects a locale change.
  void HandleLocaleChange();

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container);

  DeskButton* GetDeskButton() const;

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

 private:
  class DelegateView;

  // Returns the proper origin that the shrunk desk button should have to be
  // centered in the shelf.
  gfx::Point GetCenteredOrigin() const;

  // Sets the desk button to not be hovered and set un-expanded if necessary
  // before focusing out.
  void FocusOut(bool reverse);

  // views::Widget:
  bool OnNativeWidgetActivationChanged(bool active) override;

  raw_ptr<DelegateView, DanglingUntriaged> delegate_view_ = nullptr;

  gfx::Rect target_bounds_;

  raw_ptr<Shelf> const shelf_;
  bool is_horizontal_shelf_;
  bool is_expanded_;

  // When true, the default focus of the desk button widget is the last
  // focusable child.
  bool default_last_focusable_child_ = false;
};

}  // namespace ash

#endif  // ASH_SHELF_DESK_BUTTON_WIDGET_H_
