// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_views_delegate.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/shell.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ui/frame/frame_utils.h"

namespace ash {

namespace {

void ProcessAcceleratorNow(const ui::Accelerator& accelerator) {
  ash::AcceleratorController::Get()->Process(accelerator);
}

}  // namespace

AshTestViewsDelegate::AshTestViewsDelegate() = default;

AshTestViewsDelegate::~AshTestViewsDelegate() = default;

void AshTestViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  if (!params->parent && !params->context)
    params->context = Shell::GetRootWindowForNewWindows();

  if (params->opacity == views::Widget::InitParams::WindowOpacity::kInferred)
    chromeos::ResolveInferredOpacity(params);

  TestViewsDelegate::OnBeforeWidgetInit(params, delegate);
}

views::TestViewsDelegate::ProcessMenuAcceleratorResult
AshTestViewsDelegate::ProcessAcceleratorWhileMenuShowing(
    const ui::Accelerator& accelerator) {
  if (ash::AcceleratorController::Get()->OnMenuAccelerator(accelerator)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(ProcessAcceleratorNow, accelerator));
    return views::ViewsDelegate::ProcessMenuAcceleratorResult::CLOSE_MENU;
  }

  ProcessAcceleratorNow(accelerator);
  return views::ViewsDelegate::ProcessMenuAcceleratorResult::LEAVE_MENU_OPEN;
}

bool AshTestViewsDelegate::ShouldCloseMenuIfMouseCaptureLost() const {
  // This is the same behaviour as `ChromeViewsDelegate`.
  auto* capture_mode_test_delegate = GetTestDelegate();
  CHECK(capture_mode_test_delegate);
  return !capture_mode_test_delegate->is_session_active();
}

}  // namespace ash
