// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_VIDEO_UI_LAUNCHER_H_
#define ASH_AMBIENT_AMBIENT_VIDEO_UI_LAUNCHER_H_

#include <memory>

#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_weather_controller.h"
#include "ash/constants/ambient_video.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class PrefService;

namespace ash {

class AmbientViewDelegate;

// Launches |AmbientTheme::kVideo|. Pulls the current |AmbientVideo| selected
// from pref storage when it's time to render the video.
class AmbientVideoUiLauncher : public AmbientUiLauncher {
 public:
  explicit AmbientVideoUiLauncher(PrefService* pref_service,
                                  AmbientViewDelegate* view_delegate);
  AmbientVideoUiLauncher(const AmbientVideoUiLauncher&) = delete;
  AmbientVideoUiLauncher& operator=(const AmbientVideoUiLauncher&) = delete;
  ~AmbientVideoUiLauncher() override;

  // AmbientUiLauncher overrides:
  void Initialize(InitializationCallback on_done) override;
  std::unique_ptr<views::View> CreateView() override;
  void Finalize() override;
  AmbientBackendModel* GetAmbientBackendModel() override;
  AmbientPhotoController* GetAmbientPhotoController() override;

 private:
  void SetVideoHtmlPath(InitializationCallback on_done,
                        base::FilePath video_html_path);

  AmbientVideo current_video_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<AmbientViewDelegate> view_delegate_;
  std::unique_ptr<AmbientWeatherController::ScopedRefresher> weather_refresher_;
  base::FilePath video_html_path_;
  base::WeakPtrFactory<AmbientVideoUiLauncher> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_VIDEO_UI_LAUNCHER_H_
