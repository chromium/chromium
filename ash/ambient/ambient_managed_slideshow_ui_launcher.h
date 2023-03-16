// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_MANAGED_SLIDESHOW_UI_LAUNCHER_H_
#define ASH_AMBIENT_AMBIENT_MANAGED_SLIDESHOW_UI_LAUNCHER_H_

#include "ash/ambient/ambient_managed_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/callback_forward.h"
namespace ash {

class AmbientManagedSlideshowUiLauncher : public AmbientUiLauncher,
                                          public AmbientBackendModelObserver {
 public:
  explicit AmbientManagedSlideshowUiLauncher(AmbientViewDelegateImpl* delegate);
  AmbientManagedSlideshowUiLauncher(const AmbientManagedSlideshowUiLauncher&) =
      delete;
  AmbientManagedSlideshowUiLauncher& operator=(
      const AmbientManagedSlideshowUiLauncher&) = delete;
  ~AmbientManagedSlideshowUiLauncher() override;

  // AmbientBackendModelObserver
  void OnImagesReady() override;

  // AmbientUiLauncher overrides
  void Initialize(base::OnceClosure on_done) override;

  std::unique_ptr<views::View> CreateView() override;

  void Finalize() override;

  AmbientBackendModel* GetAmbientBackendModel() override;

  bool IsActive() override;

 private:
  friend class AmbientAshTestBase;
  AmbientManagedPhotoController photo_controller_;
  base::raw_ptr<AmbientViewDelegateImpl> delegate_;
  base::OnceClosure initialization_callback_;
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_MANAGED_SLIDESHOW_UI_LAUNCHER_H_
