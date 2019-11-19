// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
class FocusCycler;
class Shelf;

// The View for the status area widget.
class ASH_EXPORT StatusAreaWidgetDelegate : public views::AccessiblePaneView,
                                            public views::WidgetDelegate,
                                            public ShelfConfig::Observer {
 public:
  explicit StatusAreaWidgetDelegate(Shelf* shelf);
  ~StatusAreaWidgetDelegate() override;

  // Called whenever layout might change (e.g. alignment changed).
  void UpdateLayout();

  // Sets the focus cycler.
  void SetFocusCyclerForTesting(const FocusCycler* focus_cycler);

  // If |reverse|, indicates backward focusing, otherwise forward focusing.
  // Returns true if status area widget delegate should focus out on the
  // designated focusing direction, otherwise false.
  bool ShouldFocusOut(bool reverse);

  // Overridden from views::AccessiblePaneView.
  View* GetDefaultFocusableChild() override;

  // Overridden from views::View:
  const char* GetClassName() const override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::WidgetDelegate overrides:
  bool CanActivate() const override;
  void DeleteDelegate() override;

  // Overridden from ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

 protected:
  // Overridden from views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

 private:
  void UpdateWidgetSize();

  // Sets a border on |child|. If |extend_border_to_edge| is true, then an extra
  // wide border is added to extend the view's hit region to the edge of the
  // screen.
  void SetBorderOnChild(views::View* child, bool extend_border_to_edge);

  Shelf* const shelf_;
  const FocusCycler* focus_cycler_for_testing_;

  // When true, the default focus of the status area widget is the last
  // focusable child.
  bool default_last_focusable_child_ = false;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_DELEGATE_H_
