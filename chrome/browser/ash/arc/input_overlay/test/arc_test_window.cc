// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "ash/constants/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/screen.h"

namespace arc {
namespace input_overlay {
namespace test {

ArcTestWindow::ArcTestWindow(exo::test::ExoTestHelper* helper,
                             aura::Window* root,
                             const std::string& package_name) {
  surface_ = std::make_unique<exo::Surface>();
  buffer_ = std::make_unique<exo::Buffer>(
      helper->CreateGpuMemoryBuffer(gfx::Size(100, 100)));
  shell_surface_ =
      helper->CreateClientControlledShellSurface(surface_.get(), false);
  surface_->Attach(buffer_.get());

  auto display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root).id();
  shell_surface_->SetBounds(display_id, gfx::Rect(10, 10, 100, 100));
  surface_->Commit();
  shell_surface_->GetWidget()->Show();
  shell_surface_->GetWidget()->Activate();
  surface_->SetApplicationId(package_name.c_str());
  shell_surface_->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kAppType, static_cast<int>(ash::AppType::ARC_APP));
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
}  // namespace input_overlay
}  // namespace arc
