// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DESK_BUTTON_WIDGET_H_
#define ASH_SHELF_DESK_BUTTON_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_component.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class DeskButtonContainer;
class Shelf;
enum class ShelfAlignment;

// The desk button provides an overview of existing desks and quick access to
// them. The button is only visible in clamshell mode and disappears when in
// overview.
class ASH_EXPORT DeskButtonWidget : public ShelfComponent,
                                    public views::Widget {
 public:
  // Delegate view for laying out the desk button UI. It does not use the
  // default fill layout since the desk button UI has dynamic size, and the
  // widget reserves the maximum possible space for the current shelf alignment
  // and zero state.
  class DelegateView : public views::WidgetDelegateView {
   public:
    DelegateView();
    DelegateView(const DelegateView&) = delete;
    DelegateView& operator=(const DelegateView&) = delete;
    ~DelegateView() override;

    DeskButtonContainer* desk_button_container() const {
      return desk_button_container_;
    }

    // Initializes the view. Must be called before any meaningful UIs can be
    // laid out.
    void Init(DeskButtonWidget* desk_button_widget);

    // views::WidgetDelegateView:
    bool CanActivate() const override;
    void Layout(PassKey) override;

    // views::View:
    bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

   private:
    raw_ptr<DeskButtonContainer> desk_button_container_ = nullptr;
    raw_ptr<DeskButtonWidget> desk_button_widget_ = nullptr;
  };

  explicit DeskButtonWidget(Shelf* shelf);
  DeskButtonWidget(const DeskButtonWidget&) = delete;
  DeskButtonWidget& operator=(const DeskButtonWidget&) = delete;
  ~DeskButtonWidget() override;

  // Returns the max length for the widget for the horizontal or vertical shelf.
  static int GetMaxLength(bool horizontal_shelf);

  DelegateView* delegate_view() const { return delegate_view_; }

  Shelf* shelf() const { return shelf_; }

  // Indicates if the shelf should reserve some space for this widget.
  bool ShouldReserveSpaceFromShelf() const;

  // Whether the desk button should currently be visible.
  bool ShouldBeVisible() const;

  // Updates expanded state and values impacted by shelf alignment change.
  void PrepareForAlignmentChange();

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

  // Called when shelf layout manager detects a locale change.
  void HandleLocaleChange();

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container);

  DeskButtonContainer* GetDeskButtonContainer() const;

  // Returns true if this widget belongs to a horizontal shelf.
  bool IsHorizontalShelf() const;

  void SetDefaultChildToFocus(views::View* default_child_to_focus);

  // Stores the current focused view for desk button widget.
  void StoreDeskButtonFocus();

  // Restores focus to the stored focused view of desk button widget if there is
  // one.
  void RestoreDeskButtonFocus();

  // Depending on what child view has focus, either focus out of the desk
  // button, or pass the focus to the next view. `reverse` indicates backward
  // focusing, otherwise forward focusing.
  void MaybeFocusOut(bool reverse);

 private:
  // views::Widget:
  bool OnNativeWidgetActivationChanged(bool active) override;

  raw_ptr<DelegateView, DanglingUntriaged> delegate_view_ = nullptr;

  gfx::Rect target_bounds_;

  raw_ptr<Shelf> const shelf_;

  // Default child view to focus when `OnNativeWidgetActivationChanged()`
  // occurs. When it's not null, it should point to the desk button, the
  // previous desk button, or the next desk button.
  raw_ptr<views::View> default_child_to_focus_ = nullptr;

  // Stored focused view for the widget. This is used to restore the focus to
  // the desk button when the desk bar is closed. When it's not null, it should
  // point to the desk button, the previous desk button, or the next desk
  // button.
  raw_ptr<views::View> stored_focused_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_DESK_BUTTON_WIDGET_H_
