// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"

#include "ash/public/cpp/window_properties.h"
#include "components/exo/test/shell_surface_builder.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/screen.h"

namespace arc::input_overlay {
namespace test {

ArcTestWindow::ArcTestWindow(exo::test::ExoTestHelper* helper,
                             aura::Window* root,
                             const std::string& package_name,
                             const gfx::Rect bounds) {
  shell_surface_ = exo::test::ShellSurfaceBuilder({100, 100})
                       .SetApplicationId(package_name.c_str())
                       .BuildClientControlledShellSurface();
  auto display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root).id();
  shell_surface_->SetBounds(display_id, bounds);
  surface_ = shell_surface_->root_surface();
  surface_->Commit();
  shell_surface_->GetWidget()->GetNativeWindow()->SetProperty(
      ash::kArcPackageNameKey, package_name);
}

ArcTestWindow::~ArcTestWindow() = default;

aura::Window* ArcTestWindow::GetWindow() {
  return shell_surface_->GetWidget()->GetNativeWindow();
}

void ArcTestWindow::SetMinimized() {
  shell_surface_->SetMinimized();
  surface_->Commit();
}

void ArcTestWindow::SetBounds(display::Display& display, gfx::Rect bounds) {
  auto display_id = display.id();
  shell_surface_->SetBounds(display_id, bounds);
  surface_->Commit();
}

}  // namespace test
}  // namespace arc::input_overlay
