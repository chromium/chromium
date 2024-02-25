// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/arc_task_window_builder.h"

#include "ash/public/cpp/window_properties.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/shell_surface_util.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {

ArcTaskWindowBuilder::ArcTaskWindowBuilder() {
  init_params_.bounds = gfx::Rect(5, 5, 20, 20);
  init_params_.context = nullptr;
}

ArcTaskWindowBuilder& ArcTaskWindowBuilder::SetPackageName(
    std::string_view package_name) {
  package_name_ = package_name;
  return *this;
}

ArcTaskWindowBuilder& ArcTaskWindowBuilder::SetTitle(std::string_view title) {
  title_ = title;
  return *this;
}

ArcTaskWindowBuilder& ArcTaskWindowBuilder::SetShellRootSurface(
    exo::Surface* shell_root_surface) {
  shell_root_surface_ = shell_root_surface;
  return *this;
}

ArcTaskWindowBuilder& ArcTaskWindowBuilder::SetTaskId(int task_id) {
  DCHECK_GT(task_id, 0);
  task_id_ = task_id;
  return *this;
}

ArcTaskWindowBuilder::~ArcTaskWindowBuilder() = default;

views::Widget* ArcTaskWindowBuilder::BuildOwnedByNativeWidget() {
  views::Widget* widget = new views::Widget();

  init_params_.ownership =
      views::Widget::InitParams::Ownership::NATIVE_WIDGET_OWNS_WIDGET;
  Prepare(widget);

  return widget;
}

std::unique_ptr<views::Widget> ArcTaskWindowBuilder::BuildOwnsNativeWidget() {
  auto widget = std::make_unique<views::Widget>();

  init_params_.ownership =
      views::Widget::InitParams::Ownership::WIDGET_OWNS_NATIVE_WIDGET;
  Prepare(widget.get());

  return widget;
}

void ArcTaskWindowBuilder::Prepare(views::Widget* widget) {
  DCHECK(!built_);
  built_ = true;

  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->SetOwnedByWidget(true);
  delegate->SetTitle(base::ASCIIToUTF16(title_));
  init_params_.delegate = delegate.release();

  widget->Init(std::move(init_params_));
  // Set ARC id before showing the window to be recognized in
  // AppServiceAppWindowShelfController.
  exo::SetShellApplicationId(
      widget->GetNativeWindow(),
      base::StringPrintf("org.chromium.arc.%d", task_id_));
  widget->GetNativeWindow()->SetProperty(
      ash::kArcPackageNameKey,
      package_name_.empty() ? "an.arc.pkg.T3ST" : package_name_);
  if (shell_root_surface_) {
    exo::SetShellRootSurface(widget->GetNativeWindow(), shell_root_surface_);
  }
}

}  // namespace arc
