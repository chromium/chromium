// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"

#include "ash/constants/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc::input_overlay {

std::unique_ptr<views::Widget> CreateArcWindow(
    aura::Window* root_window,
    const gfx::Rect& bounds,
    const std::string& package_name) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = bounds;
  params.context = root_window;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->widget_delegate()->SetCanResize(true);
  widget->SetBounds(bounds);
  widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, std::string("app_id"));
  widget->GetNativeWindow()->SetProperty(
      aura::client::kAppType, static_cast<int>(ash::AppType::ARC_APP));
  widget->GetNativeWindow()->SetProperty(ash::kArcPackageNameKey, package_name);
  widget->Show();
  widget->Activate();

  return widget;
}

}  // namespace arc::input_overlay
