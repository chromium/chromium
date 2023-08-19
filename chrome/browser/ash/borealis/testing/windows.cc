// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/testing/windows.h"

#include "ash/test/test_widget_builder.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "components/exo/shell_surface_util.h"

namespace borealis {

ScopedTestWindow::ScopedTestWindow(std::unique_ptr<aura::Window> window,
                                   BorealisWindowManager* manager)
    : instance_id_(base::UnguessableToken::Create()),
      window_(std::move(window)),
      manager_(manager) {
  apps::Instance instance(manager_->GetShelfAppId(window_.get()), instance_id_,
                          window_.get());
  manager_->OnInstanceUpdate(apps::InstanceUpdate(nullptr, &instance));
}

ScopedTestWindow::~ScopedTestWindow() {
  apps::Instance instance(manager_->GetShelfAppId(window_.get()), instance_id_,
                          window_.get());
  std::unique_ptr<apps::Instance> delta = instance.Clone();
  delta->UpdateState(apps::InstanceState::kDestroyed, base::Time::Now());
  manager_->OnInstanceUpdate(apps::InstanceUpdate(&instance, delta.get()));
}

std::unique_ptr<aura::Window> MakeWindow(std::string name) {
  auto win = std::make_unique<aura::Window>(nullptr);
  win->Init(ui::LAYER_NOT_DRAWN);
  exo::SetShellApplicationId(win.get(), name);
  return win;
}

std::unique_ptr<ScopedTestWindow> MakeAndTrackWindow(
    std::string name,
    BorealisWindowManager* manager) {
  return std::make_unique<ScopedTestWindow>(MakeWindow(std::move(name)),
                                            manager);
}

std::unique_ptr<views::Widget> CreateFakeWidget(std::string name,
                                                bool fullscreen /*=false*/) {
  ash::TestWidgetBuilder builder;
  builder.SetShow(false);
  std::unique_ptr<views::Widget> widget = builder.BuildOwnsNativeWidget();
  exo::SetShellApplicationId(widget->GetNativeWindow(), name);
  if (fullscreen) {
    widget->SetFullscreen(true);
  }
  widget->Show();
  return widget;
}

}  // namespace borealis
