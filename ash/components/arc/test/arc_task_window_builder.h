// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_ARC_TASK_WINDOW_BUILDER_H_
#define ASH_COMPONENTS_ARC_TEST_ARC_TASK_WINDOW_BUILDER_H_

#include "components/exo/surface.h"
#include "ui/views/widget/widget.h"

namespace arc {

// Creates a window which will be recognized by code as an ARC window.
class ArcTaskWindowBuilder {
 public:
  ArcTaskWindowBuilder();
  ~ArcTaskWindowBuilder();

  // Caller retains ownership of the surface.
  ArcTaskWindowBuilder& SetShellRootSurface(exo::Surface* shell_root_surface);

  // Sets task ID of the built widget. Defaults to a constant, so will not
  // change if multiple windows are built.
  ArcTaskWindowBuilder& SetTaskId(int task_id);

  // Sets the package name of the app. If this is not supplied, a const default
  // will be used.
  ArcTaskWindowBuilder& SetPackageName(std::string_view package_name);

  // Sets the window title. If this is not supplied, a const default
  // will be used.
  ArcTaskWindowBuilder& SetTitle(std::string_view title);

  // See documentation on TestWidgetBuilder::Build{Owns,OwnedBy}NativeWidget.
  std::unique_ptr<views::Widget> BuildOwnsNativeWidget();
  views::Widget* BuildOwnedByNativeWidget();

 private:
  void Prepare(views::Widget* widget);

  bool built_{false};
  int task_id_{1};
  raw_ptr<exo::Surface> shell_root_surface_{nullptr};
  views::Widget::InitParams init_params_{
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET};
  std::string package_name_;
  std::string title_ = "ArcTaskWindowDefaultTitle";
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_ARC_TASK_WINDOW_BUILDER_H_
