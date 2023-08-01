// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"

#include "ash/constants/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
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
  // `aura::client::kAppType` property should be assigned before widget init.
  // It simulates the situation that
  // `AppServiceAppWindowShelfController::OnWindowInitialized()` is called
  // before `ArcInputOverlayManager::OnWindowInitialized()`;
  params.init_properties_container.SetProperty(
      aura::client::kAppType, static_cast<int>(ash::AppType::ARC_APP));
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->widget_delegate()->SetCanResize(true);
  widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, std::string("app_id"));
  widget->GetNativeWindow()->SetProperty(ash::kArcPackageNameKey, package_name);
  widget->Show();
  widget->Activate();

  return widget;
}

// Make sure the tasks run synchronously when creating the window.
std::unique_ptr<views::Widget> CreateArcWindowSyncAndWait(
    base::test::TaskEnvironment* task_environment,
    aura::Window* root_window,
    const gfx::Rect& bounds,
    const std::string& package_name) {
  task_environment->RunUntilIdle();
  auto window = CreateArcWindow(root_window, bounds, package_name);
  // I/O takes time here.
  task_environment->FastForwardBy(kIORead);
  return window;
}

void CheckActions(TouchInjector* injector,
                  size_t expect_size,
                  const std::vector<ActionType>& expect_types,
                  const std::vector<int>& expect_ids) {
  CHECK(injector) << "The touch injector should be not nullptr.";
  CHECK_EQ(expect_size, expect_types.size())
      << "Expected size for expect_types: " << expect_size;
  CHECK_EQ(expect_size, expect_ids.size())
      << "Expected size for expect_ids: " << expect_size;

  EXPECT_EQ(expect_size, injector->actions().size());
  for (size_t i = 0; i < expect_size; i++) {
    EXPECT_EQ(expect_types[i], injector->actions()[i]->GetType());
    EXPECT_EQ(expect_ids[i], injector->actions()[i]->id());
  }
}

}  // namespace arc::input_overlay
