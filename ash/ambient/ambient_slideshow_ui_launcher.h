// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_SLIDESHOW_UI_LAUNCHER_H_
#define ASH_AMBIENT_AMBIENT_SLIDESHOW_UI_LAUNCHER_H_

#include <memory>

#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/ambient_weather_controller.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/public/cpp/session/session_observer.h"

namespace ash {

// Launches |AmbientTheme::kSlideshow|.
class AmbientSlideshowUiLauncher : public AmbientUiLauncher,
                                   public AmbientBackendModelObserver,
                                   public SessionObserver {
 public:
  explicit AmbientSlideshowUiLauncher(AmbientViewDelegateImpl* view_delegate);
  AmbientSlideshowUiLauncher(const AmbientSlideshowUiLauncher&) = delete;
  AmbientSlideshowUiLauncher& operator=(const AmbientSlideshowUiLauncher&) =
      delete;
  ~AmbientSlideshowUiLauncher() override;

  // AmbientBackendModelObserver overrides:
  void OnImagesReady() override;
  void OnImagesFailed() override;

  // AmbientUiLauncher overrides:
  void Initialize(InitializationCallback on_done) override;
  std::unique_ptr<views::View> CreateView() override;
  void Finalize() override;
  AmbientBackendModel* GetAmbientBackendModel() override;
  AmbientPhotoController* GetAmbientPhotoController() override;

 private:
  InitializationCallback initialization_callback_;
  AmbientPhotoController photo_controller_;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observer_{this};
  const raw_ptr<AmbientViewDelegateImpl> view_delegate_;
  std::unique_ptr<AmbientWeatherController::ScopedRefresher> weather_refresher_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_SLIDESHOW_UI_LAUNCHER_H_
