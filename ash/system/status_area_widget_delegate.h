// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/system/status_area_widget.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
class FocusCycler;
class Shelf;

// The View for the status area widget.
class ASH_EXPORT StatusAreaWidgetDelegate : public views::AccessiblePaneView,
                                            public views::WidgetDelegate {
 public:
  // Prevents this delegate from calculating bounds and creating layout
  // managers.
  class PauseCalculatingTargetBounds {
   public:
    explicit PauseCalculatingTargetBounds(StatusAreaWidgetDelegate* delegate)
        : delegate_(delegate) {
      delegate->set_is_adding_tray_buttons(true);
    }
    PauseCalculatingTargetBounds(const PauseCalculatingTargetBounds&) = delete;
    PauseCalculatingTargetBounds& operator=(
        const PauseCalculatingTargetBounds&) = delete;
    ~PauseCalculatingTargetBounds() {
      delegate_->set_is_adding_tray_buttons(false);
    }

   private:
    StatusAreaWidgetDelegate* const delegate_;
  };

  explicit StatusAreaWidgetDelegate(Shelf* shelf);

  StatusAreaWidgetDelegate(const StatusAreaWidgetDelegate&) = delete;
  StatusAreaWidgetDelegate& operator=(const StatusAreaWidgetDelegate&) = delete;

  ~StatusAreaWidgetDelegate() override;

  // Calculates the bounds that this view should have given its constraints,
  // but does not actually update bounds yet.
  void CalculateTargetBounds();

  // Returns the bounds that this view should have given its constraints.
  gfx::Rect GetTargetBounds() const;

  // Performs the actual changes in bounds for this view to match its target
  // bounds.
  void UpdateLayout(bool animate);

  // Sets the focus cycler.
  void SetFocusCyclerForTesting(const FocusCycler* focus_cycler);

  // If |reverse|, indicates backward focusing, otherwise forward focusing.
  // Returns true if status area widget delegate should focus out on the
  // designated focusing direction, otherwise false.
  bool ShouldFocusOut(bool reverse);

  // Called by StatusAreaWidget when its collapse state changes.
  void OnStatusAreaCollapseStateChanged(
      StatusAreaWidget::CollapseState new_collapse_state);

  // Clears most of the Widget to prevent destruction problems before ~Widget.
  void Shutdown();

  // views::AccessiblePaneView:
  View* GetDefaultFocusableChild() override;
  const char* GetClassName() const override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::WidgetDelegate:
  bool CanActivate() const override;

  // Pauses `CalculateTargetBounds` within the calling scope.
  std::unique_ptr<PauseCalculatingTargetBounds>
  CreateScopedPauseCalculatingTargetBounds();

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

 protected:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

 private:
  // Sets a border on |child|. If |extend_border_to_edge| is true, then an extra
  // wide border is added to extend the view's hit region to the edge of the
  // screen.
  void SetBorderOnChild(views::View* child, bool extend_border_to_edge);

  // Updates `is_adding_tray_buttons_`.
  void set_is_adding_tray_buttons(bool is_adding_tray_buttons) {
    is_adding_tray_buttons_ = is_adding_tray_buttons;
  }

  Shelf* const shelf_;
  const FocusCycler* focus_cycler_for_testing_;
  gfx::Rect target_bounds_;

  // When true, the default focus of the status area widget is the last
  // focusable child.
  bool default_last_focusable_child_ = false;

  // When true, 'CalculateTargetBounds' is locked to prevent re-creating layout
  // managers.
  bool is_adding_tray_buttons_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
