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
#include "base/location.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "ui/base/mojom/window_show_state.mojom.h"
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
  // If data fetch failed, notify caller immediately without showing the widget.
  if (data_fetch_failed_.has_value() && data_fetch_failed_.value()) {
    std::move(animation_played_callback_).Run();
    return;
  }

  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = new views::WidgetDelegate;  // Takes ownership.
  params.delegate->SetOwnedByWidget(true);
  // Allow maximize so the booting container's FillLayoutManager can
  // fill the screen with the widget. This is required even for
  // fullscreen widgets.
  params.delegate->SetCanMaximize(true);
  params.delegate->SetCanFullscreen(true);
  params.name = "BootingAnimationWidget";
  params.show_state = ui::mojom::WindowShowState::kFullscreen;
  // Create the Booting Animation widget on the primary display.
  auto* animation_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_BootingAnimationContainer);
  params.parent = animation_window;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Make the opacity `kTranslucent` so the OOBE WebUI will be rendered in the
  // background.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_->Init(std::move(params));
  widget_->SetContentsView(std::make_unique<BootingAnimationView>());
  // Show widget even if the animation isn't ready yet. This prevents other UI
  // to be shown.
  widget_->Show();
}

void BootingAnimationController::ShowAnimationWithEndCallback(
    base::OnceClosure callback) {
  animation_played_callback_ = std::move(callback);
  // Show the widget early to prevent UI blinks. The animation will start once
  // its data is fetched and device is ready.
  Show();

  // Don't wait for GPU to be ready in non-ChromeOS environment.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    IgnoreGpuReadiness();
    return;
  }

  // If we are still waiting for the signal from DisplayConfigurator wait for
  // not more than a few seconds and play the animation anyway.
  if (!IsDeviceReady()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BootingAnimationController::IgnoreGpuReadiness,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta(base::Seconds(5)));
    return;
  }
  StartAnimation();
}

void BootingAnimationController::Finish() {
  widget_.reset();
  animation_played_callback_.Reset();
}

base::WeakPtr<BootingAnimationController>
BootingAnimationController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BootingAnimationController::OnDisplayConfigurationChanged(
    const display::DisplayConfigurator::DisplayStateList& displays) {
  if (!is_gpu_ready_) {
    return;
  }

  scoped_display_configurator_observer_.Reset();
  CHECK(IsDeviceReady());
  if (!animation_played_callback_.is_null()) {
    StartAnimation();
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
    data_fetch_failed_ = true;
    // Notify caller immediately that there is no animation file.
    if (!animation_played_callback_.is_null()) {
      std::move(animation_played_callback_).Run();
    }
    return;
  }

  data_fetch_failed_ = false;
  animation_data_ = std::move(data);

  // Only start if we haven't exited earlier already and the device is ready to
  // show.
  if (!animation_played_callback_.is_null() && IsDeviceReady()) {
    StartAnimation();
  }
}

void BootingAnimationController::StartAnimation() {
  if (!data_fetch_failed_.has_value()) {
    LOG(ERROR) << "Booting animation isn't ready yet.";
    return;
  }

  CHECK(!animation_played_callback_.is_null() && IsDeviceReady());
  if (was_shown_) {
    return;
  }
  was_shown_ = true;

  BootingAnimationView* view =
      static_cast<BootingAnimationView*>(widget_->GetContentsView());
  view->SetAnimatedImage(animation_data_);
  // If there is no animated image set at this point it means that data file
  // is invalid and we need to finish the animation immediately.
  auto* animated_image = view->GetAnimatedImage();
  if (!animated_image) {
    std::move(animation_played_callback_).Run();
    return;
  }

  // Observe animation to know when it finishes playing.
  scoped_animation_observer_.Observe(animated_image);
  view->Play();
}

void BootingAnimationController::IgnoreGpuReadiness() {
  // Don't do anything if the device is ready.
  if (IsDeviceReady()) {
    return;
  }
  LOG(ERROR) << "Ignore the readiness of the GPU and play the animation.";

  is_gpu_ready_ = true;
  scoped_display_configurator_observer_.Reset();
  if (!animation_played_callback_.is_null()) {
    StartAnimation();
  }
}

bool BootingAnimationController::IsDeviceReady() const {
  return is_gpu_ready_ && !scoped_display_configurator_observer_.IsObserving();
}

}  // namespace ash
