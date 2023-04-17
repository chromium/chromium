// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_managed_slideshow_ui_launcher.h"

#include <vector>

#include "ash/ambient/ambient_managed_photo_controller.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_managed_photo_source.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"

namespace ash {

AmbientManagedSlideshowUiLauncher::AmbientManagedSlideshowUiLauncher(
    AmbientViewDelegateImpl* view_delegate)
    : photo_controller_(*view_delegate,
                        CreateAmbientManagedSlideshowPhotoConfig()),
      delegate_(view_delegate) {
  ambient_backend_model_observer_.Observe(
      photo_controller_.ambient_backend_model());
  CHECK(AmbientManagedPhotoSource::Get());
  AmbientManagedPhotoSource::Get()->SetScreensaverImagesUpdatedCallback(
      base::BindRepeating(
          &AmbientManagedSlideshowUiLauncher::UpdateImageFilePaths,
          weak_factory_.GetWeakPtr()));
}
AmbientManagedSlideshowUiLauncher::~AmbientManagedSlideshowUiLauncher() =
    default;

void AmbientManagedSlideshowUiLauncher::Initialize(
    InitializationCallback on_done) {
  initialization_callback_ = std::move(on_done);
  if (!AmbientManagedPhotoSource::Get()) {
    LOG(WARNING) << "AmbientManagedPhotoSource not present. Probably "
                    "AmbientManagedPhotoController screen update is being "
                    "started during a shutdown";
    std::move(initialization_callback_).Run(/*success=*/false);
    return;
  }
  photo_controller_.UpdateImageFilePaths(
      AmbientManagedPhotoSource::Get()->GetScreensaverImages());
  photo_controller_.StartScreenUpdate();
}

void AmbientManagedSlideshowUiLauncher::UpdateImageFilePaths(
    const std::vector<base::FilePath>& path_to_images) {
  photo_controller_.UpdateImageFilePaths(path_to_images);
}

std::unique_ptr<views::View> AmbientManagedSlideshowUiLauncher::CreateView() {
  return std::make_unique<PhotoView>(delegate_,
                                     /*peripheral_ui_visible=*/false);
}

void AmbientManagedSlideshowUiLauncher::Finalize() {
  photo_controller_.StopScreenUpdate();
}

AmbientBackendModel*
AmbientManagedSlideshowUiLauncher::GetAmbientBackendModel() {
  return photo_controller_.ambient_backend_model();
}

void AmbientManagedSlideshowUiLauncher::OnImagesReady() {
  CHECK(initialization_callback_);
  std::move(initialization_callback_).Run(/*success=*/true);
}

bool AmbientManagedSlideshowUiLauncher::IsActive() {
  return photo_controller_.IsScreenUpdateActive();
}

bool AmbientManagedSlideshowUiLauncher::IsReady() {
  return LockScreen::HasInstance();
}

}  // namespace ash
