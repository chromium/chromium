// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_ANIMATION_UI_LAUNCHER_H_
#define ASH_AMBIENT_AMBIENT_ANIMATION_UI_LAUNCHER_H_

#include <memory>

#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/ambient_weather_controller.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_animation_frame_rate_controller.h"
#include "ash/ambient/ui/ambient_animation_progress_tracker.h"
#include "ash/public/cpp/session/session_observer.h"
#include "cc/paint/skottie_wrapper.h"

namespace ash {

// Launches |AmbientTheme::kFeelTheBreeze| or |AmbientTheme::kFloatOnBy|
// determined by the Ambient UI Settings. Display the animated screen saver with
// photos from the selected albums or the cached photos. The animation themes
// are implemented using the Skottie library that renders Lottie animation
// files.
class AmbientAnimationUiLauncher : public AmbientUiLauncher,
                                   public AmbientBackendModelObserver,
                                   public SessionObserver {
 public:
  AmbientAnimationUiLauncher(AmbientUiSettings current_ui_settings,
                             AmbientViewDelegateImpl* view_delegate);
  AmbientAnimationUiLauncher(const AmbientAnimationUiLauncher&) = delete;
  AmbientAnimationUiLauncher& operator=(const AmbientAnimationUiLauncher&) =
      delete;
  ~AmbientAnimationUiLauncher() override;

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
  const scoped_refptr<cc::SkottieWrapper> animation_;
  const raw_ptr<AmbientViewDelegateImpl> view_delegate_;

  InitializationCallback initialization_callback_;
  AmbientPhotoController photo_controller_;
  AmbientUiSettings current_ui_settings_;
  AmbientAnimationProgressTracker progress_tracker_;
  AmbientAnimationFrameRateController frame_rate_controller_;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observer_{this};
  ScopedSessionObserver session_observer_{this};
  std::unique_ptr<AmbientWeatherController::ScopedRefresher> weather_refresher_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_ANIMATION_UI_LAUNCHER_H_
