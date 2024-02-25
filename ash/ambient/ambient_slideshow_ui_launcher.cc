// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_slideshow_ui_launcher.h"

#include <memory>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/model/ambient_topic_queue_slideshow_delegate.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/shell.h"
#include "base/check.h"

namespace ash {

AmbientSlideshowUiLauncher::AmbientSlideshowUiLauncher(
    AmbientViewDelegateImpl* view_delegate)
    : photo_controller_(*view_delegate,
                        CreateAmbientSlideshowPhotoConfig(),
                        std::make_unique<AmbientTopicQueueSlideshowDelegate>()),
      view_delegate_(view_delegate) {}

AmbientSlideshowUiLauncher::~AmbientSlideshowUiLauncher() = default;

void AmbientSlideshowUiLauncher::OnImagesReady() {
  // Start screen update only if images are ready.
  CHECK(initialization_callback_);
  std::move(initialization_callback_).Run(/*success=*/true);
}

void AmbientSlideshowUiLauncher::OnImagesFailed() {
  CHECK(initialization_callback_);
  std::move(initialization_callback_).Run(/*success=*/false);
}

void AmbientSlideshowUiLauncher::Initialize(InitializationCallback on_done) {
  CHECK(on_done);
  initialization_callback_ = std::move(on_done);
  weather_refresher_ = Shell::Get()
                           ->ambient_controller()
                           ->ambient_weather_controller()
                           ->CreateScopedRefresher();
  ambient_backend_model_observer_.Observe(GetAmbientBackendModel());
  photo_controller_.StartScreenUpdate();
}

std::unique_ptr<views::View> AmbientSlideshowUiLauncher::CreateView() {
  return std::make_unique<PhotoView>(view_delegate_);
}

void AmbientSlideshowUiLauncher::Finalize() {
  photo_controller_.StopScreenUpdate();
  ambient_backend_model_observer_.Reset();
  weather_refresher_.reset();
}

AmbientBackendModel* AmbientSlideshowUiLauncher::GetAmbientBackendModel() {
  return photo_controller_.ambient_backend_model();
}

AmbientPhotoController*
AmbientSlideshowUiLauncher::GetAmbientPhotoController() {
  return &photo_controller_;
}

}  // namespace ash
