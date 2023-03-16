// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_managed_slideshow_ui_launcher.h"

#include "ash/ambient/ambient_managed_photo_controller.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/ui/photo_view.h"
#include "base/check.h"
#include "base/functional/callback.h"

namespace ash {

AmbientManagedSlideshowUiLauncher::AmbientManagedSlideshowUiLauncher(
    AmbientViewDelegateImpl* view_delegate)
    : photo_controller_(*view_delegate,
                        CreateAmbientManagedSlideshowPhotoConfig()),
      delegate_(view_delegate) {
  ambient_backend_model_observer_.Observe(
      photo_controller_.ambient_backend_model());
}
AmbientManagedSlideshowUiLauncher::~AmbientManagedSlideshowUiLauncher() =
    default;

void AmbientManagedSlideshowUiLauncher::Initialize(base::OnceClosure on_done) {
  initialization_callback_ = std::move(on_done);
  photo_controller_.StartScreenUpdate();
}

std::unique_ptr<views::View> AmbientManagedSlideshowUiLauncher::CreateView() {
  return std::make_unique<PhotoView>(delegate_);
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
  std::move(initialization_callback_).Run();
}

bool AmbientManagedSlideshowUiLauncher::IsActive() {
  return photo_controller_.IsScreenUpdateActive();
}

}  // namespace ash