// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/booting/booting_animation_controller.h"

#include <memory>

#include "ash/booting/booting_animation_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

constexpr base::FilePath::CharType kAnimationPath[] = FILE_PATH_LITERAL(
    "/usr/share/chromeos-assets/animated_splash_screen/splash_animation.json");

std::string ReadFileToString(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    LOG(WARNING) << "Failed reading file";
    result.clear();
  }

  return result;
}

}  // namespace

BootingAnimationController::BootingAnimationController() {
  CHECK(ash::Shell::Get()->display_configurator());
  scoped_display_configurator_observer_.Observe(
      ash::Shell::Get()->display_configurator());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToString, base::FilePath(kAnimationPath)),
      base::BindOnce(&BootingAnimationController::OnAnimationDataFetched,
                     weak_factory_.GetWeakPtr()));
}

BootingAnimationController::~BootingAnimationController() = default;

void BootingAnimationController::Show() {
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.delegate = new views::WidgetDelegate;  // Takes ownership.
  params.delegate->SetOwnedByWidget(true);
  // Allow maximize so the booting container's FillLayoutManager can
  // fill the screen with the widget. This is required even for
  // fullscreen widgets.
  params.delegate->SetCanMaximize(true);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "BootingAnimationWidget";
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  // Create the Booting Animation widget on the primary display.
  auto* animation_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_BootingAnimationContainer);
  params.parent = animation_window;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Make the opacity `kTranslucent` so the OOBE WebUI will be rendered in the
  // background.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_->Init(std::move(params));

  if (animation_data_.empty()) {
    LOG(ERROR) << "Booting animation isn't ready yet.";
    start_once_ready_ = true;
    return;
  }
  StartAnimation();
}

void BootingAnimationController::ShowAnimationWithEndCallback(
    base::OnceClosure callback) {
  animation_played_callback_ = std::move(callback);

  // Don't wait for GPU to be ready in non-ChromeOS environment.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    is_gpu_ready_ = true;
    scoped_display_configurator_observer_.Reset();
  }

  if (!scoped_display_configurator_observer_.IsObserving()) {
    Show();
  }
}

void BootingAnimationController::Finish() {
  widget_.reset();
  animation_played_callback_.Reset();
}

base::WeakPtr<BootingAnimationController>
BootingAnimationController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BootingAnimationController::OnDisplayModeChanged(
    const display::DisplayConfigurator::DisplayStateList& displays) {
  if (!is_gpu_ready_) {
    return;
  }

  scoped_display_configurator_observer_.Reset();
  if (!animation_played_callback_.is_null()) {
    Show();
  }
}

void BootingAnimationController::OnDisplaySnapshotsInvalidated() {
  // This call represents that GPU has returned us valid display snapshots, but
  // they are not still applied. Starting the animation before modeset happens
  // is too early and we need to wait for the next `OnDisplayModeChanged` call.
  is_gpu_ready_ = true;
}

void BootingAnimationController::AnimationCycleEnded(
    const lottie::Animation* animation) {
  // Once animation has finished playing we might delete it. Stop observation
  // here explicitly.
  scoped_animation_observer_.Reset();
  if (!animation_played_callback_.is_null()) {
    std::move(animation_played_callback_).Run();
  }
}

void BootingAnimationController::OnAnimationDataFetched(std::string data) {
  if (data.empty()) {
    LOG(ERROR) << "No booting animation file available.";
    return;
  }

  animation_data_ = std::move(data);

  if (start_once_ready_) {
    StartAnimation();
  }
}

void BootingAnimationController::StartAnimation() {
  CHECK(!animation_played_callback_.is_null() && is_gpu_ready_);
  if (was_shown_) {
    return;
  }

  was_shown_ = true;
  start_once_ready_ = false;
  BootingAnimationView* view = widget_->SetContentsView(
      std::make_unique<BootingAnimationView>(animation_data_));
  // Observe animation to know when it finishes playing.
  scoped_animation_observer_.Observe(view->GetAnimatedImage());
  widget_->Show();
  view->Play();
}

}  // namespace ash
