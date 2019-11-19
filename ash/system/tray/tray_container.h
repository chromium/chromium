// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_CONTAINER_H_
#define ASH_SYSTEM_TRAY_TRAY_CONTAINER_H_

#include "ash/public/cpp/shelf_config.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {
class Shelf;

// Base class for tray containers. Sets the border and layout. The container
// auto-resizes the widget when necessary.
class TrayContainer : public views::View, ShelfConfig::Observer {
 public:
  explicit TrayContainer(Shelf* shelf);
  ~TrayContainer() override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  void UpdateAfterShelfAlignmentChange();

  void SetMargin(int main_axis_margin, int cross_axis_margin);

 protected:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(View* child) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  const char* GetClassName() const override;

 private:
  void UpdateLayout();

  Shelf* const shelf_;

  int main_axis_margin_ = 0;
  int cross_axis_margin_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TrayContainer);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_CONTAINER_H_
