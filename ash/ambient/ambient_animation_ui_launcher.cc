// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_animation_ui_launcher.h"

#include <memory>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_topic_queue_animation_delegate.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_frame_rate_controller.h"
#include "ash/ambient/ui/ambient_animation_view.h"
#include "ash/shell.h"
#include "base/check.h"
#include "cc/paint/skottie_wrapper.h"

namespace ash {

AmbientAnimationUiLauncher::AmbientAnimationUiLauncher(
    AmbientUiSettings current_ui_settings,
    AmbientViewDelegateImpl* view_delegate)
    : animation_(AmbientAnimationStaticResources::Create(
                     AmbientUiSettings::ReadFromPrefService(
                         *Shell::Get()
                              ->session_controller()
                              ->GetPrimaryUserPrefService()),
                     /*serializable=*/false)
                     ->GetSkottieWrapper()),
      view_delegate_(view_delegate),
      photo_controller_(*view_delegate,
                        CreateAmbientAnimationPhotoConfig(
                            animation_->GetImageAssetMetadata()),
                        std::make_unique<AmbientTopicQueueAnimationDelegate>(
                            animation_->GetImageAssetMetadata())),
      current_ui_settings_(current_ui_settings),
      frame_rate_controller_(Shell::Get()->frame_throttling_controller()) {}

AmbientAnimationUiLauncher::~AmbientAnimationUiLauncher() = default;

void AmbientAnimationUiLauncher::OnImagesReady() {
  // Start screen update only if images are ready.
  CHECK(initialization_callback_);
  std::move(initialization_callback_).Run(/*success=*/true);
}

void AmbientAnimationUiLauncher::OnImagesFailed() {
  CHECK(initialization_callback_);
  std::move(initialization_callback_).Run(/*success=*/false);
}

void AmbientAnimationUiLauncher::Initialize(InitializationCallback on_done) {
  CHECK(on_done);
  initialization_callback_ = std::move(on_done);
  weather_refresher_ = Shell::Get()
                           ->ambient_controller()
                           ->ambient_weather_controller()
                           ->CreateScopedRefresher();
  ambient_backend_model_observer_.Observe(GetAmbientBackendModel());
  GetAmbientPhotoController()->StartScreenUpdate();
}

std::unique_ptr<views::View> AmbientAnimationUiLauncher::CreateView() {
  return std::make_unique<AmbientAnimationView>(
      view_delegate_, &progress_tracker_,
      AmbientAnimationStaticResources::Create(current_ui_settings_,
                                              /*serializable=*/true),
      &frame_rate_controller_);
}

void AmbientAnimationUiLauncher::Finalize() {
  photo_controller_.StopScreenUpdate();
  ambient_backend_model_observer_.Reset();
  weather_refresher_.reset();
}

AmbientBackendModel* AmbientAnimationUiLauncher::GetAmbientBackendModel() {
  return photo_controller_.ambient_backend_model();
}

AmbientPhotoController*
AmbientAnimationUiLauncher::GetAmbientPhotoController() {
  return &photo_controller_;
}

}  // namespace ash
