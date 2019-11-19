// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/photo_model_observer.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/photo_controller.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool CanStartAmbientMode() {
  return chromeos::features::IsAmbientModeEnabled() && PhotoController::Get() &&
         !ambient::util::IsShowing(LockScreen::ScreenType::kLogin);
}

}  // namespace

AmbientController::AmbientController(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {}

AmbientController::~AmbientController() {
  DestroyContainerView();
}

void AmbientController::OnWidgetDestroying(views::Widget* widget) {
  refresh_timer_.Stop();
  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;

  // If our widget is being destroyed, Assistant UI is no longer visible.
  // If Assistant UI was already closed, this is a no-op.
  assistant_controller_->ui_controller()->CloseUi(
      AssistantExitPoint::kUnspecified);
}

void AmbientController::Toggle() {
  if (container_view_)
    Stop();
  else
    Start();
}

void AmbientController::AddPhotoModelObserver(PhotoModelObserver* observer) {
  model_.AddObserver(observer);
}

void AmbientController::RemovePhotoModelObserver(PhotoModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AmbientController::Start() {
  if (!CanStartAmbientMode()) {
    // TODO(wutao): Show a toast to indicate that Ambient mode is not ready.
    return;
  }

  // CloseUi to ensure standalone Assistant Ui doesn't exist when entering
  // Ambient mode to avoid strange behavior caused by standalone Ui was
  // only hidden at that time. This will be a no-op if Ui was already closed.
  // TODO(meilinw): Handle embedded Ui.
  assistant_controller_->ui_controller()->CloseUi(
      AssistantExitPoint::kUnspecified);

  CreateContainerView();
  container_view_->GetWidget()->Show();
  RefreshImage();
}

void AmbientController::Stop() {
  DestroyContainerView();
}

void AmbientController::CreateContainerView() {
  DCHECK(!container_view_);
  container_view_ = new AmbientContainerView(this);
  container_view_->GetWidget()->AddObserver(this);
}

void AmbientController::DestroyContainerView() {
  // |container_view_|'s widget is owned by its native widget. After calling
  // CloseNow(), it will trigger |OnWidgetDestroying|, where it will set the
  // |container_view_| to nullptr.
  if (container_view_)
    container_view_->GetWidget()->CloseNow();
}

void AmbientController::RefreshImage() {
  if (!PhotoController::Get())
    return;

  if (model_.ShouldFetchImmediately()) {
    // TODO(b/140032139): Defer downloading image if it is animating.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientController::GetNextImage,
                       weak_factory_.GetWeakPtr()),
        kAnimationDuration);
  } else {
    model_.ShowNextImage();
    ScheduleRefreshImage();
  }
}

void AmbientController::ScheduleRefreshImage() {
  base::TimeDelta refresh_interval;
  if (!model_.ShouldFetchImmediately()) {
    // TODO(b/139953713): Change to a correct time interval.
    refresh_interval = base::TimeDelta::FromSeconds(5);
  }

  refresh_timer_.Start(
      FROM_HERE, refresh_interval,
      base::BindOnce(&AmbientController::RefreshImage, base::Unretained(this)));
}

void AmbientController::GetNextImage() {
  PhotoController::Get()->GetNextImage(base::BindOnce(
      &AmbientController::OnPhotoDownloaded, weak_factory_.GetWeakPtr()));
}

void AmbientController::OnPhotoDownloaded(const gfx::ImageSkia& image) {
  if (!image.isNull())
    model_.AddNextImage(image);

  ScheduleRefreshImage();
}

}  // namespace ash
