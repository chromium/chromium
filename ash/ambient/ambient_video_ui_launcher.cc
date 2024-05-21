// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_video_ui_launcher.h"

#include <string_view>
#include <utility>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/ui/ambient_video_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/ambient/util/time_of_day_utils.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"

#include "components/prefs/pref_service.h"

namespace ash {
namespace {

std::string_view GetVideoFile(AmbientVideo video) {
  switch (video) {
    case AmbientVideo::kNewMexico:
      return kTimeOfDayNewMexicoVideo;
    case AmbientVideo::kClouds:
      return kTimeOfDayCloudsVideo;
  }
}

}  // namespace

AmbientVideoUiLauncher::AmbientVideoUiLauncher(
    PrefService* pref_service,
    AmbientViewDelegate* view_delegate)
    : pref_service_(pref_service), view_delegate_(view_delegate) {
  CHECK(pref_service_);
}

AmbientVideoUiLauncher::~AmbientVideoUiLauncher() = default;

void AmbientVideoUiLauncher::Initialize(InitializationCallback on_done) {
  CHECK(on_done);
  AmbientUiSettings ui_settings =
      AmbientUiSettings::ReadFromPrefService(*pref_service_);
  CHECK(ui_settings.video())
      << "AmbientVideoUiLauncher should not be active for "
      << ambient::util::AmbientThemeToString(ui_settings.theme());
  ambient::RecordAmbientModeTopicSource(
      personalization_app::mojom::TopicSource::kVideo);
  current_video_ = *ui_settings.video();
  weather_refresher_ = Shell::Get()
                           ->ambient_controller()
                           ->ambient_weather_controller()
                           ->CreateScopedRefresher();
  GetAmbientVideoHtmlPath(
      ambient::kAmbientVideoDlcForegroundLabel,
      base::BindOnce(&AmbientVideoUiLauncher::SetVideoHtmlPath,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

std::unique_ptr<views::View> AmbientVideoUiLauncher::CreateView() {
  CHECK(!video_html_path_.empty());
  return std::make_unique<AmbientVideoView>(GetVideoFile(current_video_),
                                            video_html_path_, current_video_,
                                            view_delegate_);
}

void AmbientVideoUiLauncher::Finalize() {
  weak_factory_.InvalidateWeakPtrs();
  weather_refresher_.reset();
}

AmbientBackendModel* AmbientVideoUiLauncher::GetAmbientBackendModel() {
  return nullptr;
}

AmbientPhotoController* AmbientVideoUiLauncher::GetAmbientPhotoController() {
  return nullptr;
}

void AmbientVideoUiLauncher::SetVideoHtmlPath(InitializationCallback on_done,
                                              base::FilePath video_html_path) {
  video_html_path_ = std::move(video_html_path);
  std::move(on_done).Run(/*success=*/!video_html_path_.empty());
}

}  // namespace ash
