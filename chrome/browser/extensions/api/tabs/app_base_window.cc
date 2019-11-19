// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/size_constraints.h"

namespace extensions {

AppBaseWindow::AppBaseWindow(AppWindow* app_window) : app_window_(app_window) {}

AppBaseWindow::~AppBaseWindow() {}

bool AppBaseWindow::IsActive() const {
  return GetBaseWindow()->IsActive();
}

bool AppBaseWindow::IsMaximized() const {
  return GetBaseWindow()->IsMaximized();
}

bool AppBaseWindow::IsMinimized() const {
  return GetBaseWindow()->IsMinimized();
}

bool AppBaseWindow::IsFullscreen() const {
  return GetBaseWindow()->IsFullscreen();
}

gfx::NativeWindow AppBaseWindow::GetNativeWindow() const {
  return GetBaseWindow()->GetNativeWindow();
}

gfx::Rect AppBaseWindow::GetRestoredBounds() const {
  return GetBaseWindow()->GetRestoredBounds();
}

ui::WindowShowState AppBaseWindow::GetRestoredState() const {
  return GetBaseWindow()->GetRestoredState();
}

gfx::Rect AppBaseWindow::GetBounds() const {
  return GetBaseWindow()->GetBounds();
}

void AppBaseWindow::Show() {
  GetBaseWindow()->Show();
}

void AppBaseWindow::Hide() {
  GetBaseWindow()->Hide();
}

bool AppBaseWindow::IsVisible() const {
  return GetBaseWindow()->IsVisible();
}

void AppBaseWindow::ShowInactive() {
  GetBaseWindow()->ShowInactive();
}

void AppBaseWindow::Close() {
  GetBaseWindow()->Close();
}

void AppBaseWindow::Activate() {
  GetBaseWindow()->Activate();
}

void AppBaseWindow::Deactivate() {
  GetBaseWindow()->Deactivate();
}

void AppBaseWindow::Maximize() {
  GetBaseWindow()->Maximize();
}

void AppBaseWindow::Minimize() {
  GetBaseWindow()->Minimize();
}

void AppBaseWindow::Restore() {
  GetBaseWindow()->Restore();
}

void AppBaseWindow::SetBounds(const gfx::Rect& bounds) {
  // We constrain the given size to the min/max sizes of the
  // application window.
  gfx::Insets frame_insets = GetBaseWindow()->GetFrameInsets();
  SizeConstraints constraints(
      SizeConstraints::AddFrameToConstraints(
          GetBaseWindow()->GetContentMinimumSize(), frame_insets),
      SizeConstraints::AddFrameToConstraints(
          GetBaseWindow()->GetContentMaximumSize(), frame_insets));

  gfx::Rect new_bounds = bounds;
  new_bounds.set_size(constraints.ClampSize(bounds.size()));

  GetBaseWindow()->SetBounds(new_bounds);
}

void AppBaseWindow::FlashFrame(bool flash) {
  GetBaseWindow()->FlashFrame(flash);
}

ui::ZOrderLevel AppBaseWindow::GetZOrderLevel() const {
  return GetBaseWindow()->GetZOrderLevel();
}

void AppBaseWindow::SetZOrderLevel(ui::ZOrderLevel level) {
  GetBaseWindow()->SetZOrderLevel(level);
}

NativeAppWindow* AppBaseWindow::GetBaseWindow() const {
  return app_window_->GetBaseWindow();
}

}  // namespace extensions
